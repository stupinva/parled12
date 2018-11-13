#ifndef __MASTER__
#define __MASTER__

#include "parports.h"

/* Функция, реализующая ведущий процесс.

   Удаляет PID-файл и Unix-сокет, если они существуют,

   запускает ведомый процесс и ждёт сигналов,

   при получении сигнала CHLD, если до этого ему не были отправлены сигналы INT
   или TERM, перезапускает ведомый процесс,

   при получении сигнала INT или TERM завершает ведомый процесс, удаляет
   PID-файл и Unix-сокет, после чего завершает работу.

   pidfile_pathname - полный путь к PID-файлу или указатель NULL,
   остальные параметры аналогичны параметрам функции slave, см. файл slave.h */
int master(const char *pidfile_pathname,
           parports_t *parports,

           const char *unix_socket_pathname,
           int unix_socket_uid,
           int unix_socket_gid,
           int unix_socket_mode,

           int uid,
           int gid,
           const char *chroot_pathname);
#endif
