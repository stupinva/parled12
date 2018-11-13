#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <errno.h>
#include "daemon.h"
#include "evloop.h"

/* Структура данных содержит информацию об одном сокете, ожидающем событий */
struct socket_s
{
  int fd;                /* Файловый дескриптор сокета */
  struct socket_s *prev; /* Ссылки на предыдущий */
  struct socket_s *next; /* и следующий сокеты в списке */

  int waited_events;     /* Флаги ожидаемых событий */

  /* Функция-обработчик событий в сокете.

     Функция должна вернуть 0, если она желает корректно удалить сокет из
     списка ожидающих сокетов и освободить память, занимаемую приватными
     данными.

     Функция должна вернуть -1, если она желает сделать всё то же самое, но
     сообщает об аварийном завершении работы.

     В противном случае функция должна вернуть маску ожидаемых событий. Нужно
     обязательно указать хотя бы одно ожидаемое событие, т.к. в противном
     случае функция заведомо больше никогда не будет вызывана, из-за чего
     значение 0 будет считаться желанием функции-обработчика удалить сокет.
   */
  int (*process_event)(int fd,
                       int events,
                       void *data);

  /* Функция, освобождающая память, занятую приватными данными обработчика
     событий */
  int (*destroy)(void *data);

  void *data;            /* Приватные данные обработчика событий сокета */
};

/* Функция для создания новых сокетов, ожидающих событий. Используется для
   сокрытия внутренней структуры данных socket_s/socket_t */
socket_t *socket_create(int fd,
                        int waited_events,
                        int (*process_event)(int fd,
                                             int events,
                                             void *data),
                        int (*destroy)(void *data),
                        void *data)
{
  if (process_event == NULL)
  {
    log_message(LOG_ERR, "socket_create: process_event is NULL pointer");
    return NULL;
  }

  if (destroy == NULL)
  {
    log_message(LOG_ERR, "socket_create: destroy is NULL pointer");
    return NULL;
  }

  /* Если у сокета нет приватных данных, то это подозрительно, но
     преступлением не является. На всякий случай выводим предпреждение */
  if (data == NULL)
  {
    log_message(LOG_WARNING, "socket_create: warning, data is NULL pointer");
  }

  /* Выделяем память под ожидающий сокет */
  socket_t *socket = malloc(sizeof(socket_t));
  if (socket == NULL)
  {
    log_message(LOG_ERR, "socket_create: cannot allocate memory for socket");
    return NULL;
  }

  /* Инициализируем созданный ожидающий сокет */
  socket->fd = fd;
  socket->prev = NULL;
  socket->next = NULL;
  socket->waited_events = waited_events;
  socket->process_event = process_event;
  socket->destroy = destroy;
  socket->data = data;

  return socket;
}

/* Структура данных содержит двусвязный список сокетов, ожидающих событий и
   файловый дескриптор epoll */
struct evloop_s
{
  int ep;
  socket_t *first;
  socket_t *last;
};

/* Количество ожидающих сокетов, обрабатываемых за один проход цикла обработки
   поступивших событий */
#define MAX_EVENTS 16

/* Создать список сокетов, ожидающих события */
evloop_t *evloop_create()
{
  /* Пытаемся выделить память под список */
  evloop_t *evloop = malloc(sizeof(evloop_t));
  if (evloop == NULL)
  {
    log_message(LOG_ERR, "evloop_create: failed to allocate memory for new evloop");
    return NULL;
  }

  /* Пытаемся создать новый дескриптор epoll */
  evloop->ep = epoll_create1(0);
  if (evloop->ep == -1)
  {
    log_error(LOG_ERR, "evloop_create: failed to create epoll for new evloop");
    free(evloop);
    return NULL;
  }

  /* Список сокетов, ожидающих события, пока что пуст */
  evloop->first = NULL;
  evloop->last = NULL;

  return evloop;
}

/* Добавить сокет в список сокетов, ожидающих поступления событий */
int evloop_add_socket(evloop_t *evloop, socket_t *socket)
{
  if (evloop == NULL)
  {
    log_message(LOG_ERR, "evloop_add: evloop is NULL pointer");
    return -1;
  }

  if (socket == NULL)
  {
    log_message(LOG_ERR, "evloop_add: socket is NULL pointer");
    return -1;
  }

  /* Регистрируем этот сокет в epoll */
  struct epoll_event event;
  event.events = socket->waited_events;
  event.data.ptr = socket;
  if (epoll_ctl(evloop->ep, EPOLL_CTL_ADD, socket->fd, &event) == -1)
  {
    log_error(LOG_ERR, "evloop_add: failed to add socket to evloop");
    return -1;
  }

  /* Если список ещё пуст, добавляем сокет как единственный элемент */
  if ((evloop->first == NULL) && (evloop->last == NULL))
  {
    evloop->first = socket;
    evloop->last = socket;

    socket->prev = NULL;
    socket->next = NULL;
  }
  /* Если в списке уже есть элементы, добавляем сокет в конец списка */
  else
  {
    evloop->last->next = socket;
    socket->prev = evloop->last;
    socket->next = NULL;
    evloop->last = socket;
  }

  return 0;
}

/* Удалить сокет из списка сокетов, ожидающих поступления событий */
int evloop_delete_socket(evloop_t *evloop, socket_t *socket)
{
  if (evloop == NULL)
  {
    log_message(LOG_ERR, "evloop_delete_socket: evloop is NULL pointer");
    return -1;
  }

  if (socket == NULL)
  {
    log_message(LOG_ERR, "evloop_delete_socket: socket is NULL pointer");
    return -1;
  }

  /* Удаляем сокет из epoll */
  if (epoll_ctl(evloop->ep, EPOLL_CTL_DEL, socket->fd, NULL) == -1)
  {
    log_error(LOG_WARNING, "evloop_delete_socket: warning, failed to delete socket from evloop");
  }

  /* Если сокет был первым в списке */
  if (socket->prev == NULL)
  {
    evloop->first = socket->next;
    evloop->first->prev = NULL;
  }
  /* Если перед этим сокетом в списке есть другой */
  else
  {
    socket->prev->next = socket->next;
  }

  /* Если сокет был последним в списке */
  if (socket->next == NULL)
  {
    evloop->last = socket->prev;
    evloop->last->next = NULL;
  }
  /* Если за этим сокетом в списке есть другой */
  else
  {
    socket->next->prev = socket->prev;
  }

  /* Корректно освобождаем память, занимаемую приватными данными обработчика событий в сокете */
  if (socket->destroy(socket->data) == -1)
  {
    log_message(LOG_WARNING, "evloop_delete_socket: warning, failed to destroy socket data");
  }

  /* Закрываем файловый дескриптор сокета */
  if (close(socket->fd) == -1)
  {
    log_error(LOG_WARNING, "evloop_delete_socket: warning, failed to close socket");
  }

  return 0;
}

/* Удаление всего списка сокетов, ожидающих поступления событий */
int evloop_destroy(evloop_t *evloop)
{
  if (evloop == NULL)
  {
    log_message(LOG_ERR, "evloop_destroy: evloop is NULL pointer");
    return -1;
  }

  /* Перебираем элементы списка, удаляем сокеты из списка по одному */
  while (evloop->first != NULL)
  {
    if (evloop_delete_socket(evloop, evloop->first) == -1)
    {
      log_message(LOG_WARNING, "evloop_destroy: warning, evloop_delete_socket failed");
    }
  }

  /* Закрываем файловый дескриптор epoll */
  if (close(evloop->ep) == -1)
  {
    log_error(LOG_WARNING, "evloop_destroy: warning, close failed");
  }

  /* Освобождаем память, занимаемую структурой данных со списком ожидающих сокетов */
  free(evloop);

  return 0;
}

/* Флаг выставляется функцией-обработчиком сигналов TERM и INT. Означает необходимость
   завершить работу цикла обработки событий в сокетах */
int evloop_stop = 0;

/* Функция-обработчик сигналов TERM и INT. Выставляет флаг необходимости завершить
   цикл обработки событий в сокетах */
void evloop_sighandler(int signal)
{
  if (signal == SIGTERM)
  {
    evloop_stop = 1;
  }
  else if (signal == SIGINT)
  {
    evloop_stop = 1;
  }
}

/* Функция запускает цикл обработки событий в сокетах. Завершает цикл обработки
   событий по сигналам TERM или INT */
int evloop_run(evloop_t *evloop)
{
  if (evloop == NULL)
  {
    log_message(LOG_ERR, "evloop_run: evloop is NULL pointer");
    return -1;
  }

  /* Формируем структуру, которая описывает обработчик сигнала */
  struct sigaction new_sa;
  new_sa.sa_handler = evloop_sighandler;
  new_sa.sa_flags = SA_RESTART;

  /* Прежде чем установить новые обработчики, сбросим флаг
     необходимости завершить цикл обработки событий в сокете */
  evloop_stop = 0;

  /* Устанавливаем новый обработчик для сигналов TERM и INT,
     а старые обработчики запоминаем */
  struct sigaction old_term_sa;
  struct sigaction old_int_sa;
  sigaction(SIGTERM, &new_sa, &old_term_sa);
  sigaction(SIGINT, &new_sa, &old_int_sa);

  /* Формируем маску сигналов, которые не должны прерывать выполнение системных вызовов.
     Это все сигналы, кроме TERM и INT */
  sigset_t sigmask;
  sigfillset(&sigmask);
  sigdelset(&sigmask, SIGTERM);
  sigdelset(&sigmask, SIGINT);

  /* Входим в бесконечный цикл обработки событий, который будет прерван только по
     сигналам TERM или INT */
  while (1 == 1)
  {
    struct epoll_event events[MAX_EVENTS];

    /* Ожидем наступления событий в указанном количестве сокетов или поступления
       сигнала TERM или INT */
    int n = epoll_pwait(evloop->ep, events, MAX_EVENTS, -1, &sigmask);
    /* Если события не поступили, но работа системного вызова была прервана сигналом, то
       проверяем, поступил ли сигнал завершить цикл ожидания событий */
    if (n == -1)
    {
      /* Завершаем бесконечный цикл ожидания и обработки событий */
      if ((errno == EINTR) && (evloop_stop == 1))
      {
        break;
      }

      /* В противном случае произошла какая-то другая ошибка */
      log_error(LOG_ERR, "evloop_run: epoll_wait failed");
      return -1;
    }

    /* Обрабатываем события в каждом из сокетов, где они произошли */
    for(int i = 0; i < n; i++)
    {
      socket_t *socket = events[i].data.ptr;

      /* Запускаем обработку события */
      int result = socket->process_event(socket->fd,
                                         events[i].events,
                                         socket->data);
      /* Если результат равен -1, значит произошла ошибка в сокете */
      if (result == -1)
      {
        log_message(LOG_WARNING, "evloop_run: warning, socket process_event failed");
        if (evloop_delete_socket(evloop, socket) == -1)
        {
          log_message(LOG_WARNING, "evloop_run: warning, evloop_delete_socket failed");
        }
      }
      /* Если результат равен 0, значит сокет нужно закрыть */
      else if (result == 0)
      {
        if (evloop_delete_socket(evloop, socket) == -1)
        {
          log_message(LOG_WARNING, "evloop_run: warning, evloop_delete_socket failed");
        }
      }
      /* В противном случае результат - это события, которые сокет желает
         получать. Если они отличаются от текущего значения, то перенастраиваем
         файловый дескриптор этого сокета в epoll */
      else if (result != socket->waited_events)
      {
        struct epoll_event event;
        event.events = result;
        event.data.ptr = socket;
  
        /* Пытаемся обновить события, поступления которых ожидает сокет */
        if (epoll_ctl(evloop->ep, EPOLL_CTL_MOD, socket->fd, &event) == -1)
        {
          log_error(LOG_ERR, "evloop_run: failed to modify events, waited by socket from evloop");
          return -1;
        }

        /* Запоминаем новые события, поступления которых ожидает сокет */
        socket->waited_events = result;
      }
    }
  }

  return 0;
}
