#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "daemon.h"
#include "server.h"
#include "client.h"

/* Структура содержит информацию о сервере: указатель на структуру, содержащую
   список сокетов, ожидающих событий, где нужно регистрировать подключившихся
   клиентов и указатель на структуру с параллельными портами, который нужно
   передавать клиентам при их создании */
typedef struct server_s
{
  evloop_t *evloop;
  parports_t *parports;
} server_t;

/* Обработать события в сокете - создать клиента и добавить его сокет к
   списку сокетов, ожидающих события */
int server_process_event(int fd, int events, void *data)
{
  if (data == NULL)
  {
    log_message(LOG_ERR, "server_process_event: data is NULL pointer");
    return -1;
  }

  server_t *server = data;

  /* Обработка входящего подключения */
  if (events & EPOLLIN)
  {
    int conn = accept(fd, NULL, NULL);
    /* Если в процессе приёма входящих подключений произошла ошибка, то
       завершаем обработку события, сообщая о необходимости удалить сокет из списка
       сокетов, ожидающих поступление событий. */
    if (conn == -1)
    {
      log_error(LOG_ERR, "server_process_event: failed to accept connection");
      return -1;
    }

    /* Входящее подключение принято, создаём нового клиента */
    socket_t *client = client_create(conn, server->parports);
    if (client == NULL)
    {
      log_message(LOG_WARNING, "server_process_event: warning, client_create failed");
    }
    /* И добавляем его сокет к списку сокетов, ожидающих поступление событий */
    else if (evloop_add_socket(server->evloop, client) == -1)
    {
      log_message(LOG_WARNING, "server_process_event: warning, evloop_add_socket failed");
    }
  }

  /* Если произошла ошибка сокета, то закрываем сервер.
     Фактически это будет означать невозможность установить новое подключение,
     но уже установленные подключения будут продолжать обрабатываться */
  if (events & (EPOLLERR | EPOLLHUP))
  {
    log_message(LOG_ERR, "server_process_event: socket broken");
    return -1;
  }

  return EPOLLIN;
}

/* Освобождение памяти, занятой приватными данными сервера */
int server_destroy(void *data)
{
  if (data == NULL)
  {
    log_message(LOG_ERR, "server_destroy: data is NULL pointer");
    return -1;
  }

  free(data);
  return 0;
}

/* Создание сервера для обслуживания клиентов, управляющих светодиодами на параллельных портах */
socket_t *server_create(int fd, evloop_t *evloop, parports_t *parports)
{
  if (evloop == NULL)
  {
    log_message(LOG_ERR, "server_create: evloop is NULL pointer");
    return NULL;
  }

  if (parports == NULL)
  {
    log_message(LOG_ERR, "server_create: parports is NULL pointer");
    return NULL;
  }

  /* Выделяем память под структуру данных, где будем хранить указатели */
  server_t *server = malloc(sizeof(server_t));
  if (server == NULL)
  {
    log_message(LOG_ERR, "server_create: failed to allocate memory for server");
    return NULL;
  }

  /* Инициализируем структуру данных сервера */
  server->evloop = evloop;
  server->parports = parports;

  /* Создаём сокет, ожидающий поступления событий */
  socket_t *socket = socket_create(fd, EPOLLIN, server_process_event, server_destroy, server);
  if (socket == NULL)
  {
    log_message(LOG_ERR, "server_create: socket_create failed");
    free(server);
    return NULL;
  }

  return socket;
}
