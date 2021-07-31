#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "daemon.h"
#include "config.h"

/* Возвращает идентификатор пользователя, соответствующий указанному имени пользователя */
int get_uid(const char *user)
{
  if (user == NULL)
  {
    log_message(LOG_ERR, "get_uid: user name is NULL pointer");
    return -1;
  }

  struct passwd *pwd = getpwnam(user);
  if (pwd == NULL)
  {
    log_message(LOG_ERR, "get_uid: failed to parse user name %s to uid", user);
    return -1;
  }

  return pwd->pw_uid;
}

/* Возвращает идентификатор группы пользователей, соответствующий указанному имени группы */
int get_gid(const char *group)
{
  if (group == NULL)
  {
    log_message(LOG_ERR, "get_gid: group name is NULL pointer");
    return -1;
  }

  struct group *grp = getgrnam(group);
  if (grp == NULL)
  {
    log_message(LOG_ERR,"get_gid: failed to parse group name %s to gid", group);
    return -1;
  }

  return grp->gr_gid;
}

/* Преобразование строки с числом в знаковое длинное целое */
int parse_sl(const char *s, long *n)
{
  if (s == NULL)
  {
    log_message(LOG_ERR, "parse_sl: source string is NULL pointer");
    return -1;
  }

  if (n == NULL)
  {
    log_message(LOG_ERR, "parse_sl: target is NULL pointer");
    return -1;
  }

  /* Пропускаем пробельные символы в начале строки */
  while (isspace(s[0]))
  {
    s++;
  }

  /* Если первый символ после пробелов не является цифрой или знаком числа,
     то это не число */
  if (!isdigit(s[0]) && (s[0] != '-') && (s[0] != '+'))
  {
    log_message(LOG_ERR, "parse_sl: string starts with unexpected characters: %s", s);
    return -1;
  }

  /* Выполняем преобразование строки в число */
  errno = 0;
  char *p = (char *)s;
  *n = strtol(s, &p, 0);

  /* Анализируем ошибки переполнения */
  if (errno == ERANGE)
  {
    if (*n == LONG_MAX)
    {
      log_error(LOG_ERR, "parse_sl: too big value: %s", s);
    }
    else if (*n == LONG_MIN)
    {
      log_error(LOG_ERR, "parse_sl: too small value: %s", s);
    }
    return -1;
  }

  /* Пропускаем пробельные символы в конце строки */
  while (isspace(p[0]))
  {
    p++;
  }

  /* Если в конце строки ещё что-то осталось, то это было не число */
  if (p[0] != '\0')
  {
    log_message(LOG_ERR, "parse_ul: string ends with unexpected characters: %s", p);
    return -1;
  }

  return 0;
}

/* Преобразование строки с числом в беззнаковое длинное целое */
int parse_ul(const char *s, unsigned long *n)
{
  if (s == NULL)
  {
    log_message(LOG_ERR, "parse_ul: source string is NULL pointer");
    return -1;
  }

  if (n == NULL)
  {
    log_message(LOG_ERR, "parse_ul: target is NULL pointer");
    return -1;
  }

  /* Пропускаем пробельные символы в начале строки */
  while (isspace(s[0]))
  {
    s++;
  }

  /* Если первый символ после пробелов не является цифрой, то это не число */
  if (!isdigit(s[0])) 
  {
    log_message(LOG_ERR, "parse_ul: string starts with unexpected characters: %s", s);
    return -1;
  }

  /* Выполняем преобразование строки в число */
  errno = 0;
  char *p = (char *)s;
  *n = strtoul(s, &p, 0);

  /* Анализируем ошибки переполнения */
  if (errno == ERANGE)
  {
    log_error(LOG_ERR, "parse_ul: too big value: %s", s);
    return -1;
  }

  /* Пропускаем пробельные символы в конце строки */
  while (isspace(p[0]))
  {
    p++;
  }

  /* Если в конце строки ещё что-то осталось, то это было не число */
  if (p[0] != '\0')
  {
    log_message(LOG_ERR, "parse_ul: string ends with unexpected characters: %s", p);
    return -1;
  }

  return 0;
}

/* Преобразование строки с числом в знаковое целое */
int parse_si(const char *s, int *n)
{
  if (s == NULL)
  {
    log_message(LOG_ERR, "parse_si: source string is NULL pointer");
    return -1;
  }

  if (n == NULL)
  {
    log_message(LOG_ERR, "parse_si: target is NULL pointer");
    return -1;
  }

  /* Выполняем преобразование строки в длинное целое */
  long m;
  if (parse_sl(s, &m) == -1)
  {
    log_message(LOG_ERR, "parse_si: parse_sl failed");
    return -1;
  }

  /* Проверяем, что длинное целое не вышло за пределы целого числа */
  if (m > INT_MAX)
  {
    log_message(LOG_ERR, "parse_si: too big value");
    return -1;
  }
  else if (m < INT_MIN)
  {
    log_message(LOG_ERR, "parse_si: too small value");
    return -1;
  }

  /* Усекаем длинное целое до целого числа */
  *n = (int)m;
  return 0;
}

/* Преобразование строки с числом в беззнаковое целое */
int parse_ui(const char *s, unsigned *n)
{
  if (s == NULL)
  {
    log_message(LOG_ERR, "parse_ui: source string is NULL pointer");
    return -1;
  }

  if (n == NULL)
  {
    log_message(LOG_ERR, "parse_ui: target is NULL pointer");
    return -1;
  }

  /* Выполняем преобразование строки в беззнаковое длинное целое */
  unsigned long m;
  if (parse_ul(s, &m) == -1)
  {
    log_message(LOG_ERR, "parse_ui: parse_ul failed");
    return -1;
  }

  /* Проверяем, чтобы беззнаковое длинное целое не вышло за пределы беззнакового целого числа */
  if (m > UINT_MAX)
  {
    log_message(LOG_ERR, "parse_ui: too big value");
    return -1;
  }

  /* Усекаем беззнаковое длинное целое до беззнакового целого числа */
  *n = (unsigned)m;
  return 0;
}

/* Преобразование строки с числом, указывающим режим доступа к файлу, в режим доступа в числовом виде */
int parse_mode(const char *s)
{
  if (s == NULL)
  {
    log_message(LOG_ERR, "parse_mode: Source string is NULL pointer");
    return -1;
  }

  /* Преобразуем строку в беззнаковое целое число */
  unsigned mode;
  if (parse_ui(s, &mode) == -1)
  {
    log_message(LOG_ERR, "parse_mode: parse_ui failed");
    return -1;
  }

  /* Проверяем, чтобы беззнаковое целое не вышло за пределы маски режимов доступа */
  if (mode > 0777)
  {
    log_message(LOG_ERR, "parse_mode: too big value for chmod");
    return -1;
  }

  return (int)mode;
}

/* Функция выполняет разбор переданных аргументов и возвращает структуру со
   значениями настроек программы */
config_t *config_create(const int carg, const char **varg)
{
  /* Выделяем место для структуры с конфигурацией */
  config_t *config = malloc(sizeof(config_t));
  if (config == NULL)
  {
    log_message(LOG_ERR, "config_create: failed to allocate memory for config");
    return NULL;
  }

  /* Инициализируем структуру значениями по умолчанию */
  config->parports = parports_create();
#ifndef LITE
  config->pidfile_pathname = NULL;
#endif
  config->unix_socket_pathname = DEFAULT_SOCKET;
  config->unix_socket_uid = -1;
  config->unix_socket_gid = -1;
  config->unix_socket_mode = -1;
  config->uid = -1;
  config->gid = -1;
  config->chroot_pathname = NULL;
#ifndef LITE
  config->daemon = 0;
#endif
  config->mode = MODE_RUN;

  /* Если не удалось создать структуру со списком параллельных портов,
     то дальше продолжать нет смысла */
  if (config->parports == NULL)
  {
    log_message(LOG_ERR, "config_create: failed to create parports");
    free(config);
    return NULL;
  }

  /* Перебираем аргументы командной строки, ищем среди них названия опций и
     заполняем структуру значениями аргументов */
  for(int i=1; i < carg; i++)
  {
    /* Разбор опции, указывающей путь к ещё одному файлу устройства параллельного порта */
    if (strcmp(varg[i], "--parport") == 0)
    {
      i++;
      if (i < carg)
      {
        if (parports_add(config->parports, varg[i]) == -1)
        {
          log_message(LOG_ERR, "config_create: failed to add port to parports");
          config->mode = MODE_HELP;
          return config;
        }
      }
      else
      {
        log_message(LOG_ERR, "config_create: missing value for option --parport");
        config->mode = MODE_HELP;
        return config;
      }
    }
#ifndef LITE
    /* Разбор опции, указывающей на необходимость запуска программы в режиме демона */
    else if (strcmp(varg[i], "--daemon") == 0)
    {
      config->daemon = 1;
    }
    /* Разбор опции, указывающей путь к файлу с идентификатором процесса */
    else if (strcmp(varg[i], "--pidfile") == 0)
    {
      i++;
      if (i < carg)
      {
        config->pidfile_pathname = varg[i];
      }
      else
      {
        log_message(LOG_ERR, "config_create: missing value for option --pidfile");
        config->mode = MODE_HELP;
        return config;
      }
    }
#endif
    /* Разбор опции, указывающей путь к Unix-сокету, на который будут поступать
       входящие подключения */
    else if (strcmp(varg[i], "--socket") == 0)
    {
      i++;
      if (i < carg)
      {
        config->unix_socket_pathname = varg[i];
      }
      else
      {
        log_message(LOG_ERR, "config_create: missing value for option --socket");
        config->mode = MODE_HELP;
        return config;
      }
    }
    /* Разбор опции, указывающей владельца Unix-сокета */
    else if (strcmp(varg[i], "--socket-owner") == 0)
    {
      i++;
      if (i < carg)
      {
        config->unix_socket_uid = get_uid(varg[i]);
        if (config->unix_socket_uid == -1)
        {
          log_message(LOG_ERR, "config_create: wrong value for option --socket-owner");
          config->mode = MODE_HELP;
          return config;
        }
      }
      else
      {
        log_message(LOG_ERR, "config_create: missing value for option --socket-owner");
        config->mode = MODE_HELP;
        return config;
      }
    }
    /* Разбор опции, указывающей группу владельца Unix-сокета */
    else if (strcmp(varg[i], "--socket-group") == 0)
    {
      i++;
      if (i < carg)
      {
        config->unix_socket_gid = get_gid(varg[i]);
        if (config->unix_socket_gid == -1)
        {
          log_message(LOG_ERR, "config_create: wrong value for option --socket-group");
          config->mode = MODE_HELP;
          return config;
        }
      }
      else
      {
        log_message(LOG_ERR, "config_create: missing value for option --socket-group");
        config->mode = MODE_HELP;
        return config;
      }
    }
    /* Разбор опции, указывающей режим доступа к Unix-сокету */
    else if (strcmp(varg[i], "--socket-mode") == 0)
    {
      i++;
      if (i < carg)
      {
        config->unix_socket_mode = parse_mode(varg[i]);
        if (config->unix_socket_mode == -1)
        {
          log_message(LOG_ERR, "config_create: wrong value for option --socket-mode");
          config->mode = MODE_HELP;
          return config;
        }
      }
      else
      {
        log_message(LOG_ERR, "config_create: missing value for option --socket-mode");
        config->mode = MODE_HELP;
        return config;
      }
    }
    /* Разбор опции, указывающей имя пользователя, от имени которого должен
       выполняться ведомый процесс */
    else if (strcmp(varg[i], "--user") == 0)
    {
      i++;
      if (i < carg)
      {
        config->uid = get_uid(varg[i]);
        if (config->uid == -1)
        {
          log_message(LOG_ERR, "config_create: wrong value for option --user");
          config->mode = MODE_HELP;
          return config;
        }
      }
      else
      {
        log_message(LOG_ERR, "config_create: missing value for option --user");
        config->mode = MODE_HELP;
        return config;
      }
    }
    /* Разбор опции, указывающей группу, от имени которой должен
       выполняться ведомый процесс */
    else if (strcmp(varg[i], "--group") == 0)
    {
      i++;
      if (i < carg)
      {
        config->gid = get_gid(varg[i]);
        if (config->gid == -1)
        {
          log_message(LOG_ERR, "config_create: wrong value for option --group");
          config->mode = MODE_HELP;
          return config;
        }
      }
      else
      {
        log_message(LOG_ERR, "config_create: missing value for option --group");
        config->mode = MODE_HELP;
        return config;
      }
    }
    /* Разбор опции, указывающей путь к каталогу, который станет корневым для ведомого процесса */
    else if (strcmp(varg[i], "--chroot") == 0)
    {
      i++;
      if (i < carg)
      {
        config->chroot_pathname = varg[i];
      }
      else
      {
        log_message(LOG_ERR, "config_create: missing value for option --chroot");
        config->mode = MODE_HELP;
        return config;
      }
    }
    /* Разбор опции, которая указывает на необходимость вывести справку о программе */
    else if (strcmp(varg[i], "--help") == 0)
    {
      config->mode = MODE_HELP;
    }
    /* Найдена неизвестная опция, сообщаем об ошибке и переключаем программу в режим
       вывода справки */
    else
    {
      log_message(LOG_ERR, "config_create: wrong option: %s", varg[i]);
      config->mode = MODE_HELP;
    }
  }

  /* Если не было указано ни одного устройства параллельного порта, то
     используем одно устройство по умолчанию */
  if (parports_number(config->parports) == 0)
  {
    parports_add(config->parports, DEFAULT_PARPORT);
  }

  return config;
}

/* Функция освобождает память, занимаемую структурой с настройками программы */
int config_destroy(config_t *config)
{
  if (config == NULL)
  {
    log_message(LOG_ERR, "config_destroy: config is NULL pointer");
    return -1;
  }

  if (parports_destroy(config->parports) == -1)
  {
    log_message(LOG_WARNING, "config_destroy: warning, failed to destroy parports");
  }

  free(config);

  return 0;
}
