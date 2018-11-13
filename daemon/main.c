/* Программа-демон для управления светодиодами через драйвер параллельного
   порта пространства пользователя. Программа состоит из ведущего и
   ведомого процессов.
 
   Ведущий управляет PID-файлом, запускает ведомый процесс, перезапускает
   его при внезапном завершении, завершает его при окончании работы,
   удаляет за ним Unix-сокет.

   Ведомый процесс после запуска открывает все управляемые им файлы
   устройств параллельных портов, открывает Unix-сокет для входящих
   подключений, при необходимости сбрасывает привилегии и вход в цикл
   обработки поступающих подключений и запросов. При получении сигнала
   завершения работы от ведущего процесса выходит из цикла. */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "daemon.h"
#include "slave.h"
#include "config.h"
#ifndef LITE
#include "master.h"
#endif

int main(const int carg, const char **varg)
{
  /* Выполняем разбор аргументов командной строки в структуру, содержащую настройки программы */
  config_t *config = config_create(carg, varg);
  if (config == NULL)
  {
    log_message(LOG_ERR, "main: config is NULL pointer");
    return 1;
  }

  /* Работа в основном режиме */
  if (config->mode == MODE_RUN)
  {
#ifndef LITE
    /* Если требуется работать в режиме демона, то переходим в этот режим */
    if (config->daemon == 1)
    {
      daemonize();
    }

    /* Запускаем ведущий процесс, который запустит ведомый и будет им управлять */
    if (master(config->pidfile_pathname,
               config->parports,
               config->unix_socket_pathname, config->unix_socket_uid,
               config->unix_socket_gid, config->unix_socket_mode,
               config->uid, config->gid, config->chroot_pathname) == -1)
    {
      log_message(LOG_ERR, "main: master failed");
      return 1;
    }
#else
    /* Запускаем ведомый процесс */
    if (slave(config->parports,
              config->unix_socket_pathname, config->unix_socket_uid,
              config->unix_socket_gid, config->unix_socket_mode,
              config->uid, config->gid, config->chroot_pathname) == -1)
    {
      log_message(LOG_ERR, "main: slave failed");
      return 1;
    }
#endif
  }
  /* Работа в режиме вывода справки */
  else /* if (config->mode == MODE_HELP) */
  {
    fprintf(stderr,
         /* "--------------------------------------------------------------------------------" */
            "Usage: %s <options>\n"
            "       %s --help\n"
            "Options:\n"
            "       --parport <parport>    - path to parport device, default - %s\n"
            "                                The option can be specified multiple times.\n"
            "       --socket <socket>      - path to listen unix-socket, default - \n"
            "                                %s\n"
            "       --socket-owner <user>  - owner of socket\n"
            "       --socket-group <group> - group of socket\n"
            "       --socket-mode <mode>   - access mode for socket\n"
#ifndef LITE
            "       --daemon               - run as daemon\n"
#endif
            "       --user <user>          - switch to specified user after open all sockets\n"
            "                                and devices\n"
            "       --group <group>        - switch to specified group after open all sockets\n"
            "                                and devices\n"
            "       --chroot <path>        - change root path of process to specified path\n"
#ifndef LITE
            "       --pidfile <PID-file>   - path to file, where will be saved PID, default -\n"
#endif
            "                                none\n"
            "Modes:\n"
            "       <default> - listen commands on socket and work with leds on parallel\n"
            "                   port.\n"
            "       --help    - show this help\n",
            varg[0],
            varg[0],
            DEFAULT_PARPORT,
            DEFAULT_SOCKET);
  }

  /* Освобождаем память, которая была занята конфигурацией */
  if (config_destroy(config) == -1)
  {
    log_message(LOG_WARNING, "main: failed to destroy config.\n");
  }
  return 0;
}
