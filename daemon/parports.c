#include <stdlib.h>
#include <unistd.h>

#include "daemon.h"
#include "parports.h"

struct parports_s
{
  unsigned num;         /* Количество портов в таблице */
  unsigned max;         /* Количество записей в таблице */
  parport_t **parports; /* Таблица портов */
};

/* С этим шагом будет расти размер таблицы портов */
#define PARPORTS_CLUSTER 16

/* Создание каталога портов */
parports_t *parports_create()
{
  /* Пытаемся выделить память под структуру каталога портов */
  parports_t *parports = malloc(sizeof(parports_t));
  if (parports == NULL)
  {
    log_message(LOG_ERR, "parports_create: failed to allocate memory for parports");
    return NULL;
  }

  /* В каталоге пока нет портов */
  parports->num = 0;
  parports->max = 0;
  parports->parports = NULL;
  return parports;
}

/* Изменение размера таблицы портов в каталоге на указанный */
int parports_realloc(parports_t *parports, unsigned number)
{
  if (parports == NULL)
  {
    log_message(LOG_ERR, "parports_realloc: parports is NULL pointer");
    return -1;
  }

  /* В таблице должна быть хотя бы одна запись */
  if (number < 1)
  {
    log_message(LOG_ERR, "parports_realloc: number is not a positive integer");
    return -1;
  }

  /* Нельзя уменьшить размер таблицы до размера меньше, чем количество портов в таблице */
  if (number < parports->num)
  {
    log_message(LOG_ERR, "parports_realloc: new table size less than current");
    return -1;
  }

  /* Пытаемся поменять размер таблицы портов на указанный */
  parport_t **new = realloc(parports->parports, sizeof(parport_t *) * number);
  if (new == NULL)
  {
    log_message(LOG_ERR, "parports_realloc: failed to reallocate memory");
    return -1;
  }

  /* Запоминаем новый указатель на таблицу портов и её новый размер */
  parports->parports = new;
  parports->max = number;
  return 0;
}

/* Добавление в каталог нового порта */
int parports_add(parports_t *parports, const char *pathname)
{
  if (parports == NULL)
  {
    log_message(LOG_ERR, "parports_add: parports is NULL pointer");
    return -1;
  }

  if (pathname == NULL)
  {
    log_message(LOG_ERR, "parports_add: pathname is NULL pointer");
    return -1;
  }

  /* Если нужно добавить в таблицу портов ещё места, пытаемся сделать это */
  if (parports->num + 1 > parports->max)
  {
    if (parports_realloc(parports, parports->max + PARPORTS_CLUSTER) == -1)
    {
      log_message(LOG_ERR, "parports_add: failed to enlarge parports array");
      return -1;
    }
  }

  /* Выполняем предварительную инициализацию новой записи в таблице портов */
  parports->parports[parports->num] = parport_prepare(pathname);
  if (parports->parports[parports->num] == NULL)
  {
    log_message(LOG_ERR, "parports_add: failed to add parport");
    return -1;
  }
  parports->num++;

  return 0;
}

/* Возвращает количество портов в каталоге */
int parports_number(parports_t *parports)
{
  if (parports == NULL)
  {
    log_message(LOG_ERR, "parports_number: parports is NULL pointer");
    return 0;
  }

  return parports->num;
}

/* Открыть все порты в каталоге */
int parports_open(parports_t *parports)
{
  if (parports == NULL)
  {
    log_message(LOG_ERR, "parports_open: parports is NULL pointer");
    return -1;
  }

  /* Перебираем записи в таблице портов */
  for(unsigned i = 0; i < parports->num; i++)
  {
    /* Пытаемся открыть порт. Если не получилось, то сообщаем об ошибке */
    if (parport_open(parports->parports[i]) == -1)
    {
      log_message(LOG_ERR, "parports_open: cannot open parport %d", i);
      return -1;
    }

    /* На секунду включаем все светодиоды на открытом порту,
       чтобы обозначить их исправность */
    if (parports_leds_ctl(parports, i, LEDS_SET, 0xFFF) == -1)
    {
      log_message(LOG_WARNING, "parports_open: warning, failed to set all leds");
    }
    sleep(1);
    if (parports_leds_ctl(parports, i, LEDS_SET, 0) == -1)
    {
      log_message(LOG_WARNING, "parports_open: warning, failed to reset all leds");
    }
  }

  return 0;
}

/* Закрыть все порты в таблице, удалить каталог портов */
int parports_destroy(parports_t *parports)
{
  if (parports == NULL)
  {
    log_message(LOG_ERR, "parports_destroy: warning, parports is NULL pointer");
    return -1;
  }

  /* Перебираем порты, закрываем каждый из них */
  for(unsigned parport = 0; parport < parports->num; parport++)
  {
    if (parports->parports[parport] != NULL)
    {
      if (parport_close(parports->parports[parport]) == -1)
      {
        log_message(LOG_ERR, "parports_close: warning, failed to close port");
      }
    }
  }

  /* Освобождаем память из под таблицы портов */
  if (parports->max > 0)
  {
    free(parports->parports);
  }

  /* Освобождаем память из под каталога портов */
  free(parports);

  return 0;
}

/* Выполнить указанную операцию над портом из каталога */
int parports_leds_ctl(parports_t *parports, const unsigned parport, leds_operation_t operation, int operand)
{
  if (parports == NULL)
  {
    log_message(LOG_ERR, "parports_leds_ctl: parports is NULL pointer");
    return -1;
  }

  /* Проверяем, что среди портов имеется порт с указанным номером */
  if (parport >= parports->num)
  {
    log_message(LOG_ERR, "parports_leds_ctl: no parport with index %d", parport);
    return -1;
  } 

  /* Выполняем указанную операцию над портом из таблицы */
  int leds = parport_leds_ctl(parports->parports[parport], operation, operand);
  if (leds == -1)
  {
    log_message(LOG_ERR, "parports_leds_ctl: failed to execute operation");
    return -1;
  }

  return leds;
}
