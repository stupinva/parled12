#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include "daemon.h"

#define BUF_SIZE 1024

/* Признак, что программа работает в режиме демона. В режиме демона сообщения отправляются в syslog */
int log_daemon = 0;

/* Текстовые строки, соответствующие важности сообщений syslog от 0 - LOG_EMERG до 7 - LOG_DEBUG */
const char *strpriority[] = {
  "EMERGENCY",
  "ALERT",
  "CRITICAL",
  "ERROR",
  "WARNING",
  "NOTICE",
  "INFORMATION",
  "DEBUG"
};

/* Функция для отправки сообщений в журнал. Напрямую не вызывается,
   используется только в макросах log_error и log_message */
void logger(int priority, int err, const char *filename, const int line, const char *format, ...)
{
  /* Если важность выходит за допустимые пределы, возвращаем её в эти пределы */
  if (priority < LOG_EMERG)
  {
    priority = LOG_EMERG;
  }
  else if (priority > LOG_DEBUG)
  {
    priority = LOG_DEBUG;
  }

  /* Формируем сообщение */
  char message[BUF_SIZE + 1];
  size_t n = 0;
  va_list vl;
  va_start(vl, format);
  n = vsnprintf(message, BUF_SIZE, format, vl);
  va_end(vl);
  message[n] = '\0';

  /* Формируем полное сообщение с приоритетом, именем файла модуля, номером строки в этом файле и текстом ошибки */
  char fullmessage[BUF_SIZE + 1];
  /* Если код ошибки больше нуля, то добавляем к тексту сообщения сообщение об ошибке */
  if (err > 0)
  {
    n = snprintf(fullmessage, BUF_SIZE, "[%s %s:%d %s] %s\n", strpriority[priority], filename, line, strerror(errno), message);
  }
  else
  {
    n = snprintf(fullmessage, BUF_SIZE, "[%s %s:%d] %s\n", strpriority[priority], filename, line, message);
  }
  fullmessage[n] = '\0';

  /* Если программа находится в режиме демона, то выводим сообщение в syslog */
  if (log_daemon == 1)
  {
    syslog(priority, "%s", (char *)fullmessage);
  }
  /* В противном случае выводим сообщение в стандартный поток диагностических сообщений */
  else
  {
    fprintf(stderr, "%s", fullmessage);
  }
}

/* Переход в режим демона:
   1. переход в корневой каталог,
   2. ветвление и завершение родительского процесса,
   3. открытие новой сессии,
   4. связывание потоков стандартного ввода-вывода с пустым устройством */
int daemonize()
{
  /* Меняем текущий каталог на корневой, чтобы не мешать размонтировать
     файловые системы */
  if (chdir("/") == -1)
  {
    log_error(LOG_ERR, "daemonize: chdir to root failed");
    return -1;
  }

  /* Разветвляем процесс на два экземпляра */
  pid_t pid = fork();
  if (pid == -1)
  {
    log_error(LOG_ERR, "daemonize: fork failed");
    return -1;
  }
  /* Мы в родительском процессе */
  if (pid != 0)
  {
    exit(0);
  }
  /* Здесь продолжает работу дочерний процесс */

  /* Начинаем новую сессию */
  pid = setsid();
  if (pid == -1)
  {
    log_error(LOG_ERR, "daemonize: setsid failed");
    return -1;
  }

  /* Переключем функцию ведения журналов на syslog */
  log_daemon = 1;

  /* Закрываем стандартные потоки ввода-вывода и связываем их с бездонно-пустым
     устройством */
  fclose(stdin);
  fclose(stdout);
  fclose(stderr);
  stdin = fopen("/dev/null", "r");
  stdout = fopen("/dev/null", "w");
  stderr = fopen("/dev/null", "w");

  return 0;
}
