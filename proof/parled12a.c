/* parled12a.c */

/* Управление светодиодами через файл, в который отображаются порты
   ввода-вывода */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define LPT1 0x3BC
#define LPT2 0x378
#define LPT3 0x278

#define MODE_GET 0
#define MODE_SET 1
#define MODE_HELP 2

int ports_open()
{
  int f = open("/dev/port", O_RDWR, 0);
  if (f == -1)
  {
    fprintf(stderr, "ports_open: Cannot open file /dev/port\n");
    return -1;
  }

  return f;
}

int port_seek(int f, unsigned port)
{
  off_t offset = lseek(f, port, SEEK_SET);
  if (offset != port)
  {
    fprintf(stderr, "port_seek: Cannot seek file /dev/port to offset 0x%04X\n", port);
    return -1;
  }

  return 0;
}

int port_write(int f, unsigned port, unsigned char value)
{
  if (port_seek(f, port))
  {
    fprintf(stderr, "set_port: port_seek failed.\n");
    return -1;
  }

  size_t size = write(f, &value, sizeof(value));
  if (size != sizeof(value))
  {
    fprintf(stderr, "port_write: Cannot write value to file /dev/port\n");
    return -1;
  }

  return 0;
}

int port_read(int f, unsigned port)
{
  if (port_seek(f, port))
  {
    fprintf(stderr, "port_read: port_seek failed.\n");
    return -1;
  }

  unsigned char value = 0;
  size_t size = read(f, &value, sizeof(value));
  if (size != sizeof(value))
  {
    fprintf(stderr, "port_read: Cannot read value from file /dev/port\n");
    return -1;
  }

  return value;
}

int leds_set(unsigned data_port, unsigned value)
{
  int f = ports_open();
  if (f == -1)
  {
    fprintf(stderr, "leds_set: ports_open failed.\n");
    return -1;
  }

  /* Заменяем значения на порту данных */
  unsigned char data_bits = value & 0xFF;
  if (port_write(f, data_port, data_bits) == -1)
  {
    fprintf(stderr, "leds_set: Cannot write data bits.\n");
    close(f);
    return -1;
  }

  /* Считываем текущее значение из порта управления */
  unsigned control_port = data_port + 2;
  int control_bits = port_read(f, control_port);
  if (control_bits == -1)
  {
    fprintf(stderr, "leds_set: Cannot read control bits.\n");
    close(f);
    return -1;
  }
  /* Сбрасываем в ноль те биты, значения которых собираемся заменить */
  control_bits &= 0xF0;

  /* Заменяем значения младших 4 бит */
  control_bits |= ((value & 0x0F00) ^ 0x0B00) >> 8;

  /* Записываем значение в порт управления */
  if (port_write(f, control_port, (unsigned char)control_bits) == -1)
  {
    fprintf(stderr, "leds_set: Cannot write control bits.\n");
    close(f);
    return -1;
  }

  close(f);
  return 0;
}

int leds_get(unsigned data_port)
{
  int f = ports_open();
  if (f == -1)
  {
    fprintf(stderr, "leds_get: ports_open failed.\n");
    return -1;
  }

  /* Считываем текущее значение из порта данных */
  int data_bits = port_read(f, data_port);
  if (data_bits == -1)
  {
    fprintf(stderr, "leds_get: Cannot read data bits.\n");
    close(f);
    return -1;
  }

  /* Считываем текущее значение из порта управления */
  unsigned control_port = data_port + 2;
  int control_bits = port_read(f, control_port);
  if (control_bits == -1)
  {
    fprintf(stderr, "leds_get: Cannot read control bits.\n");
    close(f);
    return -1;
  }

  close(f);

  /* Вычисляем светящиеся светодиоды */
  int value = data_bits;
  value |= ((control_bits & 0x0F) ^ 0x0B) << 8;

  return value;
}

int get_port(const char *s)
{
  unsigned port = LPT1;

  if (strcmp(s, "LPT1") == 0)
  {
    port = LPT1;
  }
  else if (strcmp(s, "LPT2") == 0)
  {
    port = LPT2;
  }
  else if (strcmp(s, "LPT3") == 0)
  {
    port = LPT3;
  }
  else
  {
    char *p = (char *)s;
    port = strtoul(s, &p, 0);   
    if ((port == 0) && (p[0] != '\0'))
    {
      fprintf(stderr, "get_port: Cannot parse port number.\n");
      return -1;
    }
  }

  return port;
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
  unsigned data_port = LPT1;
  unsigned leds_value = 0;

  for(int i=1; i < carg; i++)
  {
    if (strcmp(varg[i], "--port") == 0)
    {
      i++;
      if (i < carg)
      {
        int port = get_port(varg[i]);
        if (port == -1)
        {
          fprintf(stderr, "Wrong value for option --port: %s.\n", varg[i]);
          return 1;
        }
        data_port = (unsigned)port;
      }
      else
      {
        fprintf(stderr, "Missing value for option --port.\n");
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
    int value = leds_get(data_port);
    if (value == -1)
    {
      fprintf(stderr, "leds_get failed.\n");
      return 2;
    }
    fprintf(stdout, "0x%04X\n", value);
  }
  else if (mode == MODE_SET)
  {
    if (leds_set(data_port, leds_value) == -1)
    {
      fprintf(stderr, "leds_set failed.\n");
      return 3;
    }
  }
  else
  {
    fprintf(stderr,
            "Usage: %s [--port <port>] --set <value>\n"
            "       %s [--port <port>] --get\n"
            "       %s [--port <port>] --help\n"
            "Options:\n"
            "       --port <port>  - IO port numberic address or one of special values:\n"
            "                        LPT1 - 0x3BC (default),\n"
            "                        LPT2 - 0x378,\n"
            "                        LPT3 - 0x278.\n"
            "Modes:\n"
            "       --set <value> - set activity of 12 leds. Value - is number, where each of\n"
            "                       12 bits represent activity of corresponding led.\n"
            "                       Special values:\n"
            "                           all - turn on all leds,\n"
            "                           none - turn off all leds.\n"
            "       --get - get activity of 12 leds. Value - is number, where each of 12 bits\n"
            "               represent activity of corresponding led.\n"
            "       --help - show this help\n",
            varg[0],
            varg[0],
            varg[0]);
    return 4;
  }

  return 0;
}
