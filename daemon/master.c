#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include "daemon.h"
#include "slave.h"
#include "config.h"
#include "master.h"

#define BUF_SIZE 4096

/* Структура данных с информацией о PID-файле */
struct pidfile_s
{
  char *pathname; /* Полный путь к PID-файлу */
  pid_t pid;      /* Идентификатор процесса */
  int fd;         /* Файловый дескриптор созданного PID-файла */
};

typedef struct pidfile_s pidfile_t;

/* Функция проверяет, активен ли процесс, идентификатор которого указан в PID-файле.
   Если процесс активен, возвращается ноль. */
int pidfile_validate(const char *pathname)
{
  if (pathname == NULL)
  {
    log_message(LOG_ERR, "pidfile_validate: pathname is NULL pointer");
    return -1;
  }

  /* Пытаемся открыть существующий файл */
  int fd = open(pathname, O_RDONLY);
  if (fd == -1)
  {
    log_message(LOG_INFO, "pidfile_validate: cannot open PID-file %s", pathname);
    return -1;
  }

  /* Пытаемся прочитать в буфер идентификатор процесса */
  char buf[BUF_SIZE + 1];
  ssize_t n = read(fd, buf, BUF_SIZE);
  if (n == -1)
  {
    log_message(LOG_WARNING, "pidfile_validate: cannot open PID-file %s", pathname);

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "pidfile_validate: warning, failed to close opened PID-file %s", pathname);
    }
    return -1;
  }
  buf[n] = '\0';

  /* Пытаемся преобразовать данные из буфера в число */
  unsigned long pid;
  if (parse_ul(buf, &pid) == -1)
  {
    log_message(LOG_WARNING, "pidfile_validate: invalid value of PID in PID-file %s", pathname);

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "pidfile_validate: warning, failed to close opened PID-file %s", pathname);
    }
    return -1;
  }

  /* Проверяем, активен ли процесс с указанным идентификатором */
  if (kill((pid_t)pid, 0) == -1)
  {
    log_message(LOG_WARNING, "pidfile_validate: no process with PID %Ld from PID-file %s", pid, pathname);

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "pidfile_validate: warning, failed to close opened PID-file %s", pathname);
    }
    return -1;
  }

  /* Все проверки пройдены, процесс активен */
  if (close(fd) == -1)
  {
    log_error(LOG_WARNING, "pidfile_validate: warning, failed to close opened PID-file %s", pathname);
  }
  return 0;
}

/* Функция создаёт PID-файл с указанным именем и указанным идентификатором в
   качестве его содержимого, если файл с указанным именем не заблокирован.

   Если файл заблокирован, то считается, что процесс уже запущен. */
pidfile_t *pidfile_create(const char *pathname, pid_t pid)
{
  if (pathname == NULL)
  {
    log_message(LOG_ERR, "pidfile_create: pathname is NULL pointer");
    return NULL;
  }

  /* Пытаемся проверить, запущен ли процесс */
  if (pidfile_validate(pathname) == 0)
  {
    log_error(LOG_ERR, "pidfile_create: process from PID-file %s is valid", pathname);
    return NULL;
  }

  /* Удаляем PID-файл, если он уже существует, но не прошёл проверку */
  if (unlink(pathname) == -1)
  {
    log_error(LOG_INFO, "pidfile_destroy: warning, failed to remove PID-file");
  }

  /* Пытаемся открыть существующий файл на запись или создать новый файл
     для записи */
  int fd = open(pathname, O_RDWR | O_CREAT | O_EXCL);
  if (fd == -1)
  {
    log_error(LOG_ERR, "pidfile_create: failed to create PID-file %s", pathname);
    return NULL;
  }

  /* Пытаемся получить эксклюзивную блокировку файла. Если не удаётся получить,
     значит демон уже активен */
  if (flock(fd, LOCK_EX) == -1)
  {
    log_error(LOG_ERR, "pidfile_create: failed to lock opened PID-file");

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "pidfile_create: warning, failed to close opened PID-file");
    }
    return NULL;
  }

  /* Формируем строчку с идентификатором текущего процесса */
  char buf[BUF_SIZE + 1];
  ssize_t n = snprintf(buf, BUF_SIZE, "%d", pid);
  if (n < 0)
  {
    log_message(LOG_ERR, "pidfile_create: failed to prepare PID");

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "pidfile_create: warning, failed to close opened PID-file");
    }
    return NULL;
  }
  buf[BUF_SIZE] = '\0';

  /* Записываем сформированную строчку в PID-файл */
  if (write(fd, buf, n) < n)
  {
    log_error(LOG_ERR, "pidfile_create: failed to write PID-file");

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "pidfile_create: warning, failed to close opened PID-file");
    }
    return NULL;
  }

  /* Выделяем память под структуру данных */
  pidfile_t *pidfile = malloc(sizeof(pidfile_t));
  if (pidfile == NULL)
  {
    log_message(LOG_ERR, "pidfile_create: failed to allocate memory for pidfile");

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "pidfile_create: warning, failed to close opened PID-file");
    }
    return NULL;
  }

  n = strlen(pathname) + 1;
  pidfile->pathname = malloc(n);
  if (pidfile->pathname == NULL)
  {
    log_message(LOG_ERR, "pidfile_create: failed to allocate memory for pathaname");
    free(pidfile);

    if (close(fd) == -1)
    {
      log_error(LOG_WARNING, "pidfile_create: warning, failed to close opened PID-file");
    }
    return NULL;
  }

  /* Запоминаем файловый дескриптор PID-файла, идентификатор процесса и полный путь к
     PID-файлу в структуре данных */
  pidfile->fd = fd;
  pidfile->pid = pid;
  strcpy(pidfile->pathname, pathname);

  return pidfile;
}

/* Закрывает PID-файл и удаляет его */
int pidfile_destroy(pidfile_t *pidfile)
{
  int result = 0;

  /* Закрываем PID-файл */
  if (close(pidfile->fd) == -1)
  {
    log_error(LOG_WARNING, "pidfile_destroy: warning, failed to close PID-file");
    result = -1;
  }

  /* Удаляем PID-файл */
  if (unlink(pidfile->pathname) == -1)
  {
    log_error(LOG_WARNING, "pidfile_destroy: warning, failed to remove PID-file");
    result = -1;
  }

  /* Освобождаем память, занимаемую структурами данных, описывающих PID-файл */
  free(pidfile->pathname);
  free(pidfile);

  return result;
}

int master_stop = 0;    /* Признак необходимости завершить работу */
int master_restart = 1; /* Признак необходимости перезапустить ведомый процесс */

/* Обработчик сигналов. При получении сигналов выставляет признаки необходимости
   завершить работу или перезапустить ведомый процесс */
void master_sighandler(int signal)
{
  if (signal == SIGTERM)
  {
    master_stop = 1;
  }
  else if (signal == SIGINT)
  {
    master_stop = 1;
  }
  else if (signal == SIGCHLD)
  {
    master_restart = 1;
  }
}

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
           const char *chroot_pathname)
{
  /* Если указан PID-файл, то пытаемся его создать */
  pidfile_t *pidfile = NULL;
  if (pidfile_pathname != NULL)
  {
    /* Получаем идентификатор текущего процесса */
    pid_t pid = getpid();

    pidfile = pidfile_create(pidfile_pathname, pid);
    if (pidfile == NULL)
    {
      log_message(LOG_ERR, "master: failed to create PID-file or master already run");
      return -1;
    }
  }

  /* Удаляем возможно существующий Unix-сокет */
  if (unlink(unix_socket_pathname) == -1)
  {
    log_error(LOG_INFO, "master: failed to remove unix-socket");
  }

  /* Готовим новый обработчик сигналов. После вызова обработчика сигнала, нужно перезапустить
     выполнение прерванного системного вызова */
  struct sigaction new_sa;
  new_sa.sa_handler = master_sighandler;
  new_sa.sa_flags = SA_RESTART;

  /* Проставляем начальные значения признаков небходимости завершить работу или
     перезапустить ведомый процесс на случай, если функция master вызвана повторно,
     а в переменных ещё хранятся значения с прошлого запуска */   
  master_stop = 0;
  master_restart = 1;

  /* Устанавливаем новый обработчик и запоминаем прежние обработчики */
  struct sigaction old_term_sa;
  struct sigaction old_int_sa;
  struct sigaction old_chld_sa;
  sigaction(SIGTERM, &new_sa, &old_term_sa);
  sigaction(SIGINT, &new_sa, &old_int_sa);
  sigaction(SIGCHLD, &new_sa, &old_chld_sa);

  /* Формируем маску сигналов. Блокируются все сигналы, кроме TERM, INT и CHLD,
     которые будут разблокировать системный вызов sigsuspend для обработки
     признаков необходимости завершить работу или перезапустить ведомый процесс,
     выставленных обработчиком сигналов */
  sigset_t sigmask;
  sigfillset(&sigmask);
  sigdelset(&sigmask, SIGTERM);
  sigdelset(&sigmask, SIGINT);
  sigdelset(&sigmask, SIGCHLD);

  /* Цикл перезапуска ведомого процесса, выход из которого осуществляется по
     сигналу SIGTERM или SIGINT - "завершить работу" */
  while (1)
  {
    pid_t pid = 0;

    /* Ведомый процесс ещё не запущен или завершился аварийно, его надо запустить */
    if ((master_stop == 0) && (master_restart == 1))
    {
      /* Разветвляем процесс на два экземпляра */
      pid = fork();
      if (pid == -1)
      {
        /* Ветвление не удалось */
        log_error(LOG_ERR, "master: fork failed");

        /* Если был создан PID-файл, пытаемся его удалить */
        if (pidfile != NULL)
        {
          if (pidfile_destroy(pidfile) == -1)
          {
            log_message(LOG_WARNING, "master: warning, failed to destory PID-file");
          }
        }
        return -1;
      }

      /* Здесь продолжает работу дочерний процесс */
      if (pid == 0)
      {
        return slave(parports,
                     unix_socket_pathname,
                     unix_socket_uid,
                     unix_socket_gid,
                     unix_socket_mode,
                     uid, gid, chroot_pathname);
      }

      /* Ведомый процесс запущен, запускать его пока что более не требуется */
      master_restart = 0;
    }

    /* Ожидаем сигналов TERM, INT или CHLD */
    sigsuspend(&sigmask);

    /* Анализируем переменные, выставленные обработчиками сигналов */

    /* Нужно завершать работу */
    if (master_stop == 1)
    {
      /* Передаём команду "завершить работу" ведомому процессу */
      kill(pid, SIGTERM);

      /* Считываем код завершения процесса, чтобы не образовался процесс-зомби */
      int status;
      wait(&status);

      fprintf(stderr, "slave exit status = %d\n", WEXITSTATUS(status));

      /* Покидаем цикл. Перезапускать ведомый процесс больше не нужно */
      break;
    }
    /* Получен сигнал о завершении ведомого процесса */
    else if (master_restart == 1)
    {
      log_message(LOG_WARNING, "master: warning, slave died");

      /* Считываем код завершения процесса, чтобы не образовался процесс-зомби */
      int status;
      wait(&status);

      fprintf(stderr, "slave exit status = %d\n", WEXITSTATUS(status));

      /* Если ведомый процесс завершился по собственной инициативе, значит перезапускать его не нужно */ 
      if (WIFEXITED(status))
      {
        break;
      }
    }
  }

  /* Восстанавливаем старые обработчики сигналов */
  sigaction(SIGCHLD, &old_chld_sa, NULL);
  sigaction(SIGINT, &old_int_sa, NULL);
  sigaction(SIGTERM, &old_term_sa, NULL);

  /* Удаляем PID-файл, если он был создан */
  if (pidfile != NULL)
  {
    if (pidfile_destroy(pidfile) == -1)
    {
      log_message(LOG_WARNING, "master: warning, failed to destory PID-file");
    }
  }

  /* Удаляем Unix-сокет, который должен был создать ведомый процесс */
  if (unlink(unix_socket_pathname) == -1)
  {
    log_error(LOG_WARNING, "master: warning, failed to remove unix-socket");
  }

  return 0;
}
