/* parled12b.c */

/* Управление светодиодами через драйвер параллельного порта пространства
   пользователя */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ppdev.h>
#include <linux/parport.h>

#define MODE_GET 0
#define MODE_SET 1
#define MODE_HELP 2

int parport_open(const char *devpath)
{
  /* Открываем файл устройства */
  int parport = open(devpath, O_RDWR);
  if (parport == -1)
  {
    fprintf(stderr, "parport_open: Cannot open %s\n", devpath);
    return -1;
  }

  /* Запрашиваем доступ к параллельному порту */
  if (ioctl(parport, PPCLAIM) == -1)
  {
    fprintf(stderr, "parport_open: Cannot claim port.\n");
    close(parport);
    return -1;
  }

  /* Согласовываем совместимости */
  int mode_compat = IEEE1284_MODE_COMPAT;
  if (ioctl(parport, PPSETMODE, &mode_compat) == -1)
  {
    fprintf(stderr, "parport_open: Cannot switch port to compatibility mode.\n");
    close(parport);
    return -1;
  }

  /* Настраиваем направление линий данных */
  int mode_write = 0;
  if (ioctl(parport, PPDATADIR, &mode_write) == -1)
  {
    fprintf(stderr, "parport_open: Cannot enable data drivers.\n");
    close(parport);
    return -1;
  }

  return parport;
}

int parport_close(int parport)
{
  if (ioctl(parport, PPRELEASE) == -1)
  {
    fprintf(stderr, "parport_close: Cannot release port.\n");
    close(parport);
    return -1;
  }

  close(parport);
  return 0;
}

int leds_set(const char *devpath, unsigned value)
{
  /* Открываем порт */
  int parport = parport_open(devpath);
  if (parport == -1)
  {
    fprintf(stderr, "leds_set: parport_open failed.\n");
    return -1;
  }

  /* Настраиваем линии данных */
  unsigned char data = value & 0xFF;
  if (ioctl(parport, PPWDATA, &data) == -1)
  {
    fprintf(stderr, "leds_set: Cannot set data bits.\n");
    parport_close(parport);
    return -1;
  }

  /* Вычисляем значение управляющих линий */
  unsigned char control = 0;
  if ((value & 0x0100) == 0)
  {
    control |= PARPORT_CONTROL_STROBE;
  }
  if ((value & 0x0200) == 0)
  {
    control |= PARPORT_CONTROL_AUTOFD;
  }
  if (value & 0x0400)
  {
    control |= PARPORT_CONTROL_INIT;
  }
  if ((value & 0x0800) == 0)
  {
    control |= PARPORT_CONTROL_SELECT;
  }

  /* Настраиваем управляющие линии */
  if (ioctl(parport, PPWCONTROL, &control) == -1)
  {
    fprintf(stderr, "leds_set: Cannot set control bits\n");
    parport_close(parport);
    return -1;
  }

  /* Закрываем параллельный порт */
  if (parport_close(parport) == -1)
  {
    fprintf(stderr, "leds_set: parport_close failed.\n");
    return -1;
  }

  return 0;
}

int leds_get(const char *devpath)
{
  /* Открываем порт */
  int parport = parport_open(devpath);
  if (parport == -1)
  {
    fprintf(stderr, "leds_get: parport_open failed.\n");
    return -1;
  }

  /* Вычисляем значение управляющих линий */
  unsigned char control = 0;
  /* Настраиваем управляющие линии */
  if (ioctl(parport, PPRCONTROL, &control) == -1)
  {
    fprintf(stderr, "leds_get: Cannot set control bits\n");
    parport_close(parport);
    return -1;
  }

  /* Считываем линии данных */
  unsigned char data = 0;
  if (ioctl(parport, PPRDATA, &data) == -1)
  {
    fprintf(stderr, "leds_get: Cannot get data bits.\n");
    parport_close(parport);
    return -1;
  }

  /* Закрываем параллельный порт */
  if (parport_close(parport) == -1)
  {
    fprintf(stderr, "leds_get: parport_close failed.\n");
    return -1;
  }

  /* Вычисляем активные светодиоды */
  int value = data;
  if ((control & PARPORT_CONTROL_STROBE) == 0)
  {
    value |= 0x0100;
  }
  if ((control & PARPORT_CONTROL_AUTOFD) == 0)
  {
    value |= 0x0200;
  }
  if (control & PARPORT_CONTROL_INIT)
  {
    value |= 0x0400;
  }
  if ((control & PARPORT_CONTROL_SELECT) == 0)
  {
    value |= 0x0800;
  }

  return value;
}

int get_value(const char *s)
{
  unsigned leds = 0x000;

  if (strcmp(s, "all") == 0)
  {
    leds = 0x0FFF;
  }
  else if (strcmp(s, "none") == 0)
  {
    leds = 0x0000;
  }
  else
  {
    char *p = (char *)s;
    leds = strtoul(s, &p, 0);   
    if ((leds == 0) && (p[0] != '\0'))
    {
      fprintf(stderr, "get_value: Cannot parse value of leds.\n");
      return -1;
    }
  }

  if (leds > 0x0FFF)
  {
    leds &= 0x0FFF;
    fprintf(stderr, "get_value: Warning, wrong leds from value has been ignored.\n");
  }

  return leds;
}

int main(const int carg, const char **varg)
{
  unsigned mode = MODE_HELP;
  const char *devpath = "/dev/parport0";
  unsigned leds_value = 0;

  for(int i=1; i < carg; i++)
  {
    if (strcmp(varg[i], "--devpath") == 0)
    {
      i++;
      if (i < carg)
      {
        devpath = varg[i];
      }
      else
      {
        fprintf(stderr, "Missing value for option --devpath.\n");
        return 1;
      }
    }
    else if (strcmp(varg[i], "--set") == 0)
    {
      i++;
      if (i < carg)
      {
        int value = get_value(varg[i]);
        if (value == -1)
        {
          fprintf(stderr, "Wrong value for option --set: %s.\n", varg[i]);
          return 1;
        }
        leds_value = (unsigned)value;
        mode = MODE_SET;
      }
      else
      {
        fprintf(stderr, "Missing value for option --set.\n");
        return 1;
      }
    }
    else if (strcmp(varg[i], "--get") == 0)
    {
      mode = MODE_GET;
    }
    else if (strcmp(varg[i], "--help") == 0)
    {
      mode = MODE_HELP;
    }
    else
    {
      fprintf(stderr, "Wrong option: %s.\n", varg[i]);
      mode = MODE_HELP;
    }
  }

  if (mode == MODE_GET)
  {
    int value = leds_get(devpath);
    if (value == -1)
    {
      fprintf(stderr, "leds_get failed.\n");
      return 2;
    }
    fprintf(stdout, "0x%04X\n", value);
  }
  else if (mode == MODE_SET)
  {
    if (leds_set(devpath, leds_value) == -1)
    {
      fprintf(stderr, "leds_set failed.\n");
      return 3;
    }
  }
  else
  {
    fprintf(stderr,
            "Usage: %s [--devpath <devpath>] --set <value>\n"
            "       %s [--devpath <devpath>] --get\n"
            "       %s --help\n"
            "Options:\n"
            "       --devpath <devpath>  - path to parport device, default - /dev/parport0\n"
            "Modes:\n"
            "       --set <value> - set activity of 12 leds. Value - is number, where each of\n"
            "                       12 bits represent activity of corresponding led.\n"
            "                       Special values:\n"
            "                           all - turn on all leds,\n"
            "                           none - turn off all leds.\n"
            "       --get - get activity of 12 leds. Value - is number, where each of 12 bits\n"
            "               represent activity of corresponding led.\n"
            "       --help - show this help\n"
            "Known bugs:\n"
            "       Option --get cannot return correct value of high 4 bits, because linux\n"
            "       driver of parallel port resets the control bits when port opened.\n",
            varg[0],
            varg[0],
            varg[0]);
    return 4;
  }

  return 0;
}
