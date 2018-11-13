#ifndef __CONFIG__
#define __CONFIG__

#include "parports.h"

/* Параллельный порт, который будет открыт по умолчанию, если в опциях командной
   строки не был указан ни один параллельный порт */
#define DEFAULT_PARPORT "/dev/parport0"

/* Путь к сокету, который будет использоваться по умолчанию, если в опциях
   командной строки не был указан другой путь */
#define DEFAULT_SOCKET "/run/parled.sock"

/* Программа может работать в одном из двух режимов:
   MODE_RUN - все аргументы были разобраны успешно,
   MODE_HELP - аргументы не указаны, либо в них есть ошибки */
typedef enum program_mode_e
{
  MODE_RUN,
  MODE_HELP
} program_mode_t;

/* Структура с информацией, добытой из аргументов командной строки */
typedef struct config_s
{
  parports_t *parports;             /* Параллельные порты, светодиодами на которых
                                       нужно управлять */

#ifndef LITE
  const char *pidfile_pathname;     /* Путь к PID-файлу - файлу с идентификатором
                                       процесса */
#endif

  const char *unix_socket_pathname; /* Путь к Unix-сокету */
  int unix_socket_uid;              /* Идентификатор владельца Unix-сокета */
  int unix_socket_gid;              /* Идентификатор группы владельца Unix-сокета */
  int unix_socket_mode;             /* Режим доступа к Unix-сокету */

  int uid;                          /* Идентификатор пользователя, от имени
                                       которого должен работать ведомый процесс */
  int gid;                          /* Идентификатор группы пользователя, от имени
                                       которой должен работать ведомый процесс */
  const char *chroot_pathname;      /* Путь к каталогу, который должен стать для
                                       ведомого процесса корневым */

#ifndef LITE
  int daemon;                       /* 0 - запуск в интерактивном режиме,
                                       1 - запуск в режиме демона */
#endif

  program_mode_t mode;              /* Режим работы программы,
                                       см. выше program_mode_t */
} config_t;

/* Функция выполняет разбор переданных аргументов и возвращает структуру со
   значениями настроек программы */
config_t *config_create(const int carg, const char **varg);

/* Функция освобождает память, занимаемую структурой с настройками программы */
int config_destroy(config_t *config);

/* Преобразование строки с числом в беззнаковое длинное целое */
int parse_ul(const char *s, unsigned long *n);

#endif
