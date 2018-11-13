#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>

#include "daemon.h"
#include "parport.h"

struct parport_s
{
  char *pathname;
  int fd;
  int leds;
};

/* Подготовка структуры с информацией о параллельном порте */
parport_t *parport_prepare(const char *pathname)
{
  if (pathname == NULL)
  {
    log_message(LOG_ERR, "parport_prepare: pathname is NULL pointer");
    return NULL;
  }

  /* Пытаемся выделить память под структуру данных о параллельном порте */
  parport_t *parport = malloc(sizeof(parport_t));
  if (parport == NULL)
  {
    log_message(LOG_ERR, "parport_prepare: failed to allocate memory for parport %s", pathname);
    return NULL;
  }

  /* Пытаемся выделить память под копию пути к файлу устройства параллельного порта */
  parport->pathname = malloc(strlen(pathname) + 1);
  if (parport->pathname == NULL)
  {
    log_message(LOG_ERR, "parport_prepare: failed to allocate memory for port pathname %s", pathname);
    free(parport);
    return NULL;
  }

  /* Если память удалось выделить, выполняем предварительную инициализацию структуры */
  strcpy(parport->pathname, pathname);
  parport->fd = -1;
  parport->leds = -1;

  return parport;
}

/* Открытие файла устройства параллельного порта, подготовка порта к работе */
int parport_open(parport_t *parport)
{
  if (parport == NULL)
  {
    log_message(LOG_ERR, "parport_open: parport is NULL pointer");
    return -1;
  }

  /* Открываем файл устройства параллельного порта по пути к нему */
  parport->fd = open(parport->pathname, O_RDWR);
  if (parport->fd == -1)
  {
    log_error(LOG_ERR, "parport_open: failed to open device file %s", parport->pathname);
    return -1;
  }

  /* Запрашиваем доступ к параллельному порту */
  if (ioctl(parport->fd, PPCLAIM) == -1)
  {
    log_error(LOG_ERR, "parport_open: failed to claim port %s", parport->pathname);
    if (close(parport->fd) == -1)
    {
      log_error(LOG_WARNING, "parport_open: warning, failed to close device file %s", parport->pathname);
    }
    parport->fd = -1;
    return -1;
  }

  /* Согласовываем режим совместимости */
  int mode_compat = IEEE1284_MODE_COMPAT;
  if (ioctl(parport->fd, PPSETMODE, &mode_compat) == -1)
  {
    log_error(LOG_ERR, "parport_open: failed to switch port %s to compatibility mode", parport->pathname);
    if (close(parport->fd) == -1)
    {
      log_error(LOG_WARNING, "parport_open: warning, failed to close device file %s", parport->pathname);
    }
    parport->fd = -1;
    return -1;
  }

  /* Настраиваем направление линий данных */
  int mode_write = 0;
  if (ioctl(parport->fd, PPDATADIR, &mode_write) == -1)
  {
    log_error(LOG_ERR, "parport_open: failed to enable data drivers on port %s", parport->pathname);
    if (close(parport->fd) == -1)
    {
      log_error(LOG_WARNING, "parport_open: warning, failed to close device file %s", parport->pathname);
    }
    parport->fd = -1;
    return -1;
  }

  return 0;
}

/* Закрытие файла устройства параллельного порта */
int parport_close(parport_t *parport)
{
  if (parport == NULL)
  {
    log_message(LOG_ERR, "parport_close: parport is NULL pointer");
    return -1;
  }

  int result = 0;

  /* Если файл устройства открыт, то освобождаем его и закрываем */
  if (parport->fd != -1)
  {
    if (ioctl(parport->fd, PPRELEASE) == -1)
    {
      log_error(LOG_WARNING, "parport_close: warning, failed to release port %s", parport->pathname);
      result = -1;
    }

    if (close(parport->fd) == -1)
    {
      log_error(LOG_WARNING, "parport_close: warning, failed to close device file %s", parport->pathname);
      result = -1;
    }
  }

  free(parport->pathname);
  free(parport);
  return result;
}

/* Выставление активности светодиодов на параллельном порту */
int parport_leds_set(parport_t *parport, unsigned leds)
{
  if (parport == NULL)
  {
    log_message(LOG_ERR, "parport_leds_set: parport is NULL pointer");
    return -1;
  }

  /* Если порт ещё не открыт, то пытаемся его открыть */
  if (parport->fd == -1)
  {
    if (parport_open(parport) == -1)
    {
      log_message(LOG_ERR, "parport_leds_set: failed to open parport %s", parport->pathname);
      return -1;
    }
  }

  /* Есть только 12 светодиодов, поэтому все биты старше игнорируем */
  if (leds > 0x0FFF)
  {
    log_message(LOG_WARNING, "parport_leds_set: warning, leds value is too big, high bits will be masked");
    leds &= 0x0FFF;
  }

  /* Выставляем состояние линий данных */
  unsigned char data = leds & 0xFF;
  if (ioctl(parport->fd, PPWDATA, &data) == -1)
  {
    log_error(LOG_ERR, "parport_leds_set: failed to set data bits on port %s", parport->pathname);
    return -1;
  }

  /* Вычисляем значение управляющих линий */
  unsigned char control = 0;
  if ((leds & 0x0100) == 0)
  {
    control |= PARPORT_CONTROL_STROBE;
  }
  if ((leds & 0x0200) == 0)
  {
    control |= PARPORT_CONTROL_AUTOFD;
  }
  if (leds & 0x0400)
  {
    control |= PARPORT_CONTROL_INIT;
  }
  if ((leds & 0x0800) == 0)
  {
    control |= PARPORT_CONTROL_SELECT;
  }

  /* Выставляем состояние управляющих линий */
  if (ioctl(parport->fd, PPWCONTROL, &control) == -1)
  {
    log_error(LOG_ERR, "parport_leds_set: failed to set control bits on port %s", parport->pathname);
    return -1;
  }

  /* Запоминаем новое состояние светодиодов в кэше */
  parport->leds = (int)leds;

  return 0;
}

/* Получение состояния активности светодиодов на параллельном порту */
int parport_leds_get(parport_t *parport)
{
  if (parport == NULL)
  {
    log_message(LOG_ERR, "parport_leds_get: parport is NULL pointer");
    return -1;
  }

  /* Если в кэше есть текущее состояние светодиодов, то сразу возвращаем его */
  if (parport->leds != -1)
  {
    return parport->leds;
  }

  /* Если порт ещё не открыт, то пытаемся его открыть */
  if (parport->fd == -1)
  {
    if (parport_open(parport) == -1)
    {
      log_message(LOG_ERR, "parport_leds_get: failed to open parport %s", parport->pathname);
      return -1;
    }
  }

  /* Считываем состояние линий данных */
  unsigned char data = 0;
  if (ioctl(parport->fd, PPRDATA, &data) == -1)
  {
    log_error(LOG_ERR, "parport_leds_get: failed to get data bits from port %s", parport->pathname);
    return -1;
  }

  /* Считываем состояние управляющих линий */
  unsigned char control = 0;
  if (ioctl(parport->fd, PPRCONTROL, &control) == -1)
  {
    log_error(LOG_ERR, "parport_leds_get: failed to get control bits from port %s", parport->pathname);
    return -1;
  }

  /* Вычисляем активные светодиоды */
  int leds = data;
  if ((control & PARPORT_CONTROL_STROBE) == 0)
  {
    leds |= 0x0100;
  }
  if ((control & PARPORT_CONTROL_AUTOFD) == 0)
  {
    leds |= 0x0200;
  }
  if (control & PARPORT_CONTROL_INIT)
  {
    leds |= 0x0400;
  }
  if ((control & PARPORT_CONTROL_SELECT) == 0)
  {
    leds |= 0x0800;
  }

  /* Запоминаем считанное и вычисленное состояние светодиодов в кэше */
  parport->leds = leds;

  return leds;
}

/* Функция для манипуляции над светодиодами на параллельном порту */
int parport_leds_ctl(parport_t *parport, leds_operation_t operation, int operand)
{
  if (parport == NULL)
  {
    log_message(LOG_ERR, "parport_leds_ctl: parport is NULL pointer");
    return -1;
  }

  /* Если для однооперандной операции указано значение операнда, отличное от -1,
     выводим предупреждение, что он будет проигнорирован */
  if ((operation == LEDS_GET) || (operation == LEDS_NOT) ||
       (operation == LEDS_INC) || (operation == LEDS_DEC))
  { 
    if (operand != -1)
    {
      log_message(LOG_WARNING, "parport_leds_ctl: warning, operand for unary operation will be ignored");
    }
  }
  /* В противном случае операнд должен быть положительным */
  else if (operand < 0)
  {
    log_message(LOG_ERR, "parport_leds_ctl: operand is negative value");
    return -1;
  }
  /* Если операция - сдвиг, то операнд должен быть меньше 12, используем только остаток от деления на 12 */
  else if (((operation == LEDS_RS) || (operation == LEDS_LS) ||
            (operation == LEDS_RCS) || (operation == LEDS_LCS)) &&
            (operand >= 12))
  {
    log_message(LOG_WARNING, "parport_leds_ctl: warning, operand is too big, remainder of division by 12 will be taken");
    operand = operand % 12;
  }
  /* В противном случае проверяем, чтобы операнд содержал не более 12 бит */
  else if (operand > 0x0FFF)
  {
    log_message(LOG_WARNING, "parport_leds_ctl: warning, operand is too big, high bits will be masked");
    operand &= 0x0FFF;
  }

  /* Если указана команда установки текущего состояния светодиодов, то выполняем её */
  if (operation == LEDS_SET)
  {
    if (parport_leds_set(parport, operand) == -1)
    {
      log_message(LOG_ERR, "parport_leds_ctl: failed to set new state of leds");
      return -1;
    }

    return operand;
  }

  /* Все остальные команды используют текущее состояние светодиодов, узнаём его */
  int leds = parport_leds_get(parport);
  if (leds == -1)
  {
    log_message(LOG_ERR, "parport_leds_ctl: failed to get current state of leds");
    return -1;
  }

  /* Выполняем запрошенную операцию над текущим состоянием светодиодов */
  switch (operation)
  {
    case LEDS_GET:
      leds = parport->leds;
      break;
    case LEDS_NOT:
      leds = ~parport->leds;
      break;
    case LEDS_OR:
      leds = parport->leds | operand;
      break;
    case LEDS_AND:
      leds = parport->leds & operand;
      break;
    case LEDS_XOR:
      leds = parport->leds ^ operand;
      break;
    case LEDS_ADD:
      leds = parport->leds + operand;
      break;
    case LEDS_SUB:
      leds = parport->leds - operand;
      break;
    case LEDS_INC:
      leds = parport->leds + 1;
      break;
    case LEDS_DEC:
      leds = parport->leds - 1;
      break;
    case LEDS_RS:
      leds = parport->leds >> operand;
      break;
    case LEDS_LS:
      leds = parport->leds << operand;
      break;
    case LEDS_RCS:
      leds = (parport->leds >> operand) |
             (parport->leds << (12 - operand));
      break;
    case LEDS_LCS:
      leds = (parport->leds << operand) |
             (parport->leds >> (12 - operand));
      break;
    default:
      log_message(LOG_ERR, "parport_leds_ctl: unknown operation was specified");
      return -1;
  }
  /* Все операции выполняются по модулю, оставляем только 12 бит */
  leds &= 0x0FFF;

  /* Выставляем новое состояние светодиодов на указанном порту */
  if (parport_leds_set(parport, leds) == -1)
  {
    log_message(LOG_ERR, "parport_leds_ctl: failed to set new state of leds");
    return -1;
  }

  return leds;
}
