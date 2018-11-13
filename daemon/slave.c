#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include "daemon.h"
#include "evloop.h"
#include "server.h"
#include "slave.h"

#define BACKLOG_NUMBER 16

/* Подготовка слушающего Unix-сокета с указанными правами доступа */
int unix_socket_create(const char *pathname, int uid, int gid, int mode,
                       unsigned int backlog)
{
  if (pathname == NULL)
  {
    log_message(LOG_ERR, "unix_socket_create: unix-socket pathname is NULL pointer");
    return -1;
  }

  /* Создаём Unix-сокет */
  int fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (fd == -1)
  {
    log_error(LOG_ERR, "unix_socket_create: failed to create unix-socket");
    return -1;
  }

  /* Готовим адрес Unix-сокета, которым является его имя */
  struct sockaddr_un address;
  address.sun_family = AF_UNIX;

  size_t len = strlen(pathname);
  if (len > sizeof(address.sun_path))
  {
    log_message(LOG_ERR, "unix_socket_create: too long unix-socket path %s", pathname);

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "unix_socket_create: failed to close listen socket");
    }
    return -1;
  }
  strncpy(address.sun_path, pathname, len);
  socklen_t size = sizeof(address) - sizeof(address.sun_path) + len;

  /* Удаляем существующий Unix-сокет или файл с именем будущего Unix-сокета */
  if (unlink(pathname) == -1)
  {
    log_error(LOG_INFO, "unix_socket_create: failed to remove unix-socket");
  }

  /* Привязываем Unix-сокет к имени */
  if (bind(fd, (struct sockaddr *)&address, size) == -1)
  {
    log_error(LOG_ERR, "unix_socket_create: failed to bind socket to path %s", pathname);

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "unix_socket_create: failed to close listen socket");
    }
    return -1;
  }

  /* Если указан владелец сокета или группа, то меняем их */
  if ((uid != -1) || (gid != -1))
  {
    if (chown(pathname, uid, gid) == -1)
    {
      log_error(LOG_ERR, "unix_socket_create: cannot chown socket");

      if (close(fd) == -1)
      {
        log_error(LOG_WARNING, "unix_socket_create: failed to close listen socket");
      }
      return -1;
    }
  }

  /* Если указан режим доступа к сокету, то меняем его */
  if (mode != -1)
  {
    if (chmod(pathname, mode) == -1)
    {
      log_error(LOG_ERR, "unix_socket_create: failed to chmod socket");

      if (close(fd) == -1)
      {
        log_error(LOG_WARNING, "unix_socket_create: failed to close listen socket");
      }
      return -1;
    }
  }

  /* Сокет будет использоваться для ожидания входящих подключений */
  if (listen(fd, backlog) == -1)
  {
    log_error(LOG_ERR, "unix_socket_create: failed to listen socket");

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "unix_socket_create: failed to close listen socket");
    }

    if (unlink(pathname) == -1)
    {
      log_error(LOG_WARNING, "unix_socket_create: failed to remove listen unix-socket");
    }
    return -1;
  }

  return fd;
}

/* Функция, реализующая ведомый процесс.

   Открывает параллельные порты,

   создаёт Unix-сокет и выставляет права дотсупа к нему (если идентификаторы
   пользователя или группы, или режим доступа отличаются от -1),

   меняет идентификаторы пользователя и группы процесса (если они отличаются
   от -1),

   меняет текущий корневой каталог (если указатель на строку с корневым
   каталогом отличается от NULL),

   затем в цикле обрабатывает поступающие подключения и запросы от клиентов.

   По сигналу INT или TERM выходит из цикла и завершает работу. */
int slave(parports_t *parports,

          const char *unix_socket_pathname,
          int unix_socket_uid,
          int unix_socket_gid,
          int unix_socket_mode,

          int uid,
          int gid,
          const char *chroot_pathname)
{
  if (parports == NULL)
  {
    log_message(LOG_ERR, "slave: parports is NULL pointer");
    return 1;
  }

  if (unix_socket_pathname == NULL)
  {
    log_message(LOG_ERR, "slave: unix_socket_pathname is NULL pointer");
    return 1;
  }

  /* Открываем параллельные порты */
  if (parports_open(parports) == -1)
  {
    log_message(LOG_ERR, "slave: parports_open failed");
    return 1;
  }

  /* Открываем Unix-сокет на прослушивание */
  int fd = unix_socket_create(unix_socket_pathname,
                              unix_socket_uid,
                              unix_socket_gid,
                              unix_socket_mode,
                              BACKLOG_NUMBER);
  if (fd == -1)
  {
    log_message(LOG_ERR, "slave: unix_socket_create failed");
    return 1;
  }

  /* Сбрасываем привилегии, если нужно */
  if (chroot_pathname != NULL)
  {
    if (chroot(chroot_pathname) == -1)
    {
      log_error(LOG_ERR, "slave: chroot failed");
      return 1;
    }
  }
  if (gid != -1)
  {
    if (setgid(gid) == -1)
    {
      log_error(LOG_ERR, "slave: setgid failed");
      return 1;
    }
  }
  if (uid != -1)
  {
    if (setuid(uid) == -1)
    {
      log_error(LOG_ERR, "slave: setuid failed");
      return 1;
    }
  }

  /* Создаём цикл обработки событий в сокетах */
  evloop_t *evloop = evloop_create();
  if (evloop == NULL)
  {
    log_message(LOG_ERR, "slave: evloop_create failed");
    return 1;
  }

  /* Создаём сервер, который будет принимать входящие
     подключения, создавать клиентов и добавлять их в
     цикл обработки событий на сокетах */
  socket_t *server = server_create(fd, evloop, parports);
  if (server == NULL)
  {
    log_message(LOG_ERR, "slave: server_create failed");
    if (evloop_destroy(evloop) == -1)
    {
      log_message(LOG_WARNING, "slave: warning, evloop_destroy failed");
    }
    return 1;
  }

  /* Добавляем сервер в цикл обработки событий на сокетах */
  if (evloop_add_socket(evloop, server) == -1)
  {
    log_message(LOG_ERR,"slave: failed to add socket to event loop");
    if (evloop_destroy(evloop) == -1)
    {
      log_message(LOG_WARNING, "slave: warning, evloop_destroy failed");
    }
    return 1;
  }

  /* Запускаем цикл обработки событий на сокетах. Эта функция завершится
     только по сигналам INT или TERM или при возникновении ошибок
     в процессе работы */
  if (evloop_run(evloop) == -1)
  {
    log_message(LOG_ERR, "slave: evloop_run failed");
  }

  /* Удаляем цикл обработки событий на сокетах */
  if (evloop_destroy(evloop) == -1)
  {
    log_error(LOG_WARNING, "slave: warning, evloop_destroy failed");
    return 1;
  }

  return 0;
}
