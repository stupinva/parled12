#ifndef __EVLOOP__
#define __EVLOOP__

/* Структура данных содержит информацию об одном сокете, ожидающем событий */
struct socket_s;
typedef struct socket_s socket_t;

/* Структура данных содержит двусвязный список сокетов, ожидающих событий и
   файловый дескриптор epoll */
struct evloop_s;
typedef struct evloop_s evloop_t;

/* Функция для создания новых сокетов, ожидающих событий. Используется для
   сокрытия внутренней структуры данных socket_s/socket_t.

   fd - файловый дескриптор сокета,
   waited_events - флаги ожидаемых событий,
   process_event - функция-обработчик событий в сокете.
     Функция должна вернуть 0, если она желает корректно удалить сокет из
     списка ожидающих сокетов и освободить память, занимаемую приватными
     данными.

     Функция должна вернуть -1, если она желает сделать всё то же самое, но
     сообщает об аварийном завершении работы.

     В противном случае функция должна вернуть маску ожидаемых событий. Нужно
     обязательно указать хотя бы одно ожидаемое событие, т.к. в противном
     случае функция заведомо больше никогда не будет вызывана, из-за чего
     значение 0 будет считаться желанием функции-обработчика удалить сокет.
  destroy - функция, освобождающая память, занятую приватными данными
     обработчика событий,
  data - Приватные данные обработчика событий сокета */
socket_t *socket_create(int fd,
                        int waited_events,
                        int (*process_event)(int fd,
                                             int events,
                                             void *data),
                        int (*destroy)(void *data),
                        void *data);

/* Создать список сокетов, ожидающих события */
evloop_t *evloop_create();

/* Добавить сокет в список сокетов, ожидающих поступления событий */
int evloop_add_socket(evloop_t *evloop, socket_t *socket);

/* Удалить сокет из списка сокетов, ожидающих поступления событий */
int evloop_delete_socket(evloop_t *evloop, socket_t *socket);

/* Удаление всего списка сокетов, ожидающих поступления событий */
int evloop_destroy(evloop_t *evloop);

/* Функция запускает цикл обработки событий в сокетах. Завершает цикл обработки
   событий по сигналам TERM или INT */
int evloop_run(evloop_t *evloop);

#endif