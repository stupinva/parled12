#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "daemon.h"
#include "evloop.h"
#include "client.h"

/* Тип распознанной команды клиента */
typedef enum
{
  CT_WRONG, /* Неправильная команда */
  CT_EXIT,  /* Команда выхода */
  CT_LEDS   /* Команда, выполняющая действия над светодиодами */
} command_type_t;

/* Тип операнда распознанной команды клиента */
typedef enum
{
  OT_NONE,  /* Операнд для операции не требуется */
  OT_BITS,  /* 12 бит, соответствующих светодиодам */
  OT_SHIFT, /* Число от 0 до 11 включительно */
} operand_type_t;

/* Распознанная команда */
typedef struct
{
  command_type_t command_type;     /* Тип команды */
  leds_operation_t leds_operation; /* Код операции над светодиодами, если operation = CT_LEDS */
  operand_type_t operand_type;     /* Тип операнда для операции над светодиодами */
  int operand;                     /* Операнд для операции над светодиодами */
  unsigned parport;                /* Номер параллельного порта в каталоге */
  char *error;                     /* Текст ошибки, если operation = CT_WRONG */
  char *rest;                      /* Нераспознанный остаток команды, если operation = CT_WRONG */
} command_t;

/* Тип аппендикса команды, задающий номер порта */
typedef enum
{
  AT_NONE,      /* Нет аппендикса */
  AT_FROM_PORT, /* Аппендикс типа from port X */
  AT_ON_PORT    /* Аппендикс типа on port X */
} appendix_type_t;

/* Структура с описанием синтаксиса команды */
typedef struct
{
  char *command_name;               /* Имя команды */
  command_type_t command_type;      /* Тип команды */
  leds_operation_t leds_operation;  /* Тип операции */
  operand_type_t operand_type;      /* Тип операнда */
  appendix_type_t appendix_type; /* Тип аппендикса команды */
} command_definition_t;

/* Описание синтаксиса всех возможных команд */
command_definition_t commands[] = {
  {"get",    CT_LEDS, LEDS_GET, OT_NONE,  AT_FROM_PORT},
  {"set",    CT_LEDS, LEDS_SET, OT_BITS,  AT_ON_PORT},
  {"not",    CT_LEDS, LEDS_NOT, OT_NONE,  AT_ON_PORT},
  {"or",     CT_LEDS, LEDS_OR,  OT_BITS,  AT_ON_PORT},
  {"and",    CT_LEDS, LEDS_AND, OT_BITS,  AT_ON_PORT},
  {"xor",    CT_LEDS, LEDS_XOR, OT_BITS,  AT_ON_PORT},
  {"add",    CT_LEDS, LEDS_ADD, OT_BITS,  AT_ON_PORT},
  {"sub",    CT_LEDS, LEDS_SUB, OT_BITS,  AT_ON_PORT},
  {"inc",    CT_LEDS, LEDS_INC, OT_NONE,  AT_ON_PORT},
  {"dec",    CT_LEDS, LEDS_DEC, OT_NONE,  AT_ON_PORT},
  {"rs",     CT_LEDS, LEDS_RS,  OT_SHIFT, AT_ON_PORT},
  {"ls",     CT_LEDS, LEDS_LS,  OT_SHIFT, AT_ON_PORT},
  {"rcs",    CT_LEDS, LEDS_RCS, OT_SHIFT, AT_ON_PORT},
  {"lcs",    CT_LEDS, LEDS_LCS, OT_SHIFT, AT_ON_PORT},
  {"exit",   CT_EXIT, LEDS_GET, OT_NONE,  AT_NONE},
  {"quit",   CT_EXIT, LEDS_GET, OT_NONE,  AT_NONE},
  {"close",  CT_EXIT, LEDS_GET, OT_NONE,  AT_NONE},
  {"logout", CT_EXIT, LEDS_GET, OT_NONE,  AT_NONE},
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(command_definition_t))

/* Проверяет строку s на совпадение начала с указанной строкой prefix.
   Если совпадение найдено, возвращается указатель на остаток строки,
   если совпадение не найдено - возвращается NULL */
char *is_prefix(char *s, char *prefix)
{
  if (s == NULL)
  {
    log_message(LOG_ERR, "is_prefix: Source string is NULL pointer");
    return NULL;
  }

  if (prefix == NULL)
  {
    log_message(LOG_ERR, "is_prefix: Prefix string is NULL pointer");
    return NULL;
  }

  size_t l = strlen(prefix);
  if (strncmp(s, prefix, l) != 0)
      return NULL;
  return &(s[l]);
}

/* Пропускаем пробельные символы в начале строки, возвращает указатель
   на первый не пробельный символ */
char *skip_spaces(char *s)
{
  if (s == NULL)
  {
    log_message(LOG_ERR, "skip_spaces: source string is NULL pointer");
    return NULL;
  }

  while (isspace(s[0]))
  {
    s++;
  }

  return s;
}

/* Разбор операнда команды и проверка его нахождения в допустимых пределах */
char *parse_operand(char *s, command_t *command)
{
  if (s == NULL)
  {
    log_message(LOG_ERR, "parse_operand: source string is NULL pointer");
    return NULL;
  }

  if (command == NULL)
  {
    log_message(LOG_ERR, "parse_operand: command is NULL pointer");
    return NULL;
  }

  /* Распознаём ключевые слова all и none */
  char *p;
  if (command->operand_type == OT_BITS)
  {
    p = is_prefix(s, "all");
    if (p != NULL)
    {
      command->operand = 0x0FFF;
      return skip_spaces(p);
    }
    else
    {
      p = is_prefix(s, "none");
      if (p != NULL)
      {
        command->operand = 0;
        return skip_spaces(p);
      }
    }
  }

  /* Распознаём числовые значения операндов */
  if ((command->operand_type == OT_BITS) || (command->operand_type == OT_SHIFT))
  {
    /* Если первый символ операнда не является цифрой, то это не число */
    if (!isdigit(s[0]))
    {
      command->command_type = CT_WRONG;
      command->leds_operation = LEDS_GET;
      command->operand_type = OT_NONE;
      command->operand = -1;
      command->parport = 0;
      command->error = "Argument <operand> starts with unexpected character";
      command->rest = s;
      return NULL;
    }

    /* Выполняем преобразование строки в число */
    errno = 0;
    p = s;
    unsigned long operand = strtoul(s, &p, 0);

    /* Проверяем выход операнда за пределы допустимых значений */
    if ((errno == ERANGE) ||
        ((command->operand_type == OT_BITS) && (operand > 0x0FFF)) ||
        ((command->operand_type == OT_SHIFT) && (operand >= 12)))
    {
      command->command_type = CT_WRONG;
      command->leds_operation = LEDS_GET;
      command->operand_type = OT_NONE;
      command->operand = -1;
      command->parport = 0;
      command->error = "Argument <operand> has too big value";
      command->rest = s;
      return NULL;
    }

    /* Запоминаем значение операнда */
    command->operand = (int)operand;
    s = p;
  }

  return skip_spaces(s);
}

/* Разбор команды в строке */
command_t parse_command(char *s)
{
  command_t command;
  char *p;

  if (s == NULL)
  {
    log_message(LOG_ERR, "parse_command: source string is NULL pointer");

    command.command_type = CT_WRONG;
    command.leds_operation = LEDS_GET;
    command.operand_type = OT_NONE;
    command.operand = -1;
    command.parport = 0;
    command.error = "Source string is NULL pointer";
    command.rest = s;

    return command;
  }

  /* Пропускаем пробельные символы в начале строки */
  s = skip_spaces(s);

  /* Пытаемся определить команду */
  unsigned i = 0;
  for(; i < NUM_COMMANDS; i++)
  {
    p = is_prefix(s, commands[i].command_name);
    if (p != NULL)
    {
      command.command_type = commands[i].command_type;
      command.leds_operation = commands[i].leds_operation;
      command.operand_type = commands[i].operand_type;
      command.operand = -1;
      command.parport = 0;
      command.error = NULL;
      command.rest = NULL;

      s = skip_spaces(p);
      break;
    }
  }

  /* Команда не распознана */
  if (i == NUM_COMMANDS)
  {
    command.command_type = CT_WRONG;
    command.leds_operation = LEDS_GET;
    command.operand_type = OT_NONE;
    command.operand = -1;
    command.parport = 0;
    command.error = "Unknown command";
    command.rest = s;

    return command;
  }

  /* Если команде нужен аргумент, то попытаемся его распознать */
  if ((command.operand_type == OT_BITS) || (command.operand_type == OT_SHIFT))
  {
    /* Если аргумент не удалось распознать, завершаем работу */
    s = parse_operand(s, &command);
    if (s == NULL)
    {
      return command;
    }
  }

  /* Если команда оперирует над светодиодами, то ищем ключевое слово leds */
  if (command.command_type == CT_LEDS)
  {
    p = is_prefix(s, "leds");
    if (p == NULL)
    {
      command.command_type = CT_WRONG;
      command.leds_operation = LEDS_GET;
      command.operand_type = OT_NONE;
      command.operand = -1;
      command.parport = 0;
      command.error = "Missing keyword 'leds'";
      command.rest = s;

      return command;
    }
    s = skip_spaces(p);
  }

  /* Если команда закончилась, значит это команда выхода или
     имеется в виду параллельный порт по умолчанию */
  if (s[0] == '\0')
  {
    return command;
  }
  /* В противном случае далее указывается номер порта,
     над которым нужно выполнить действие */

  /* Если у команды должно быть продолжение типа on port */
  if (commands[i].appendix_type == AT_ON_PORT)
  {
    p = is_prefix(s, "on");
    if (p == NULL)
    {
      command.command_type = CT_WRONG;
      command.leds_operation = LEDS_GET;
      command.operand_type = OT_NONE;
      command.operand = -1;
      command.parport = 0;
      command.error = "Missing keyword 'on'";
      command.rest = s;
      return command;
    }
    s = skip_spaces(p);
  }
  /* Если у команды должно быть продолжение типа from port */
  else if (commands[i].appendix_type == AT_FROM_PORT)
  {
    p = is_prefix(s, "from");
    if (p == NULL)
    {
      command.command_type = CT_WRONG;
      command.leds_operation = LEDS_GET;
      command.operand_type = OT_NONE;
      command.operand = -1;
      command.parport = 0;
      command.error = "Missing keyword 'from'";
      command.rest = s;
      return command;
    }
    s = skip_spaces(p);
  }

  /* Ищем ключевое слово port */
  p = is_prefix(s, "port");
  if (p == NULL)
  {
    command.command_type = CT_WRONG;
    command.leds_operation = LEDS_GET;
    command.operand_type = OT_NONE;
    command.operand = -1;
    command.parport = 0;
    command.error = "Missing keyword 'port'";
    command.rest = s;
    return command;
  }
  s = skip_spaces(p);

  /* Если первый символ операнда не является цифрой, то это не число */
  if (!isdigit(s[0]))
  {
    command.command_type = CT_WRONG;
    command.leds_operation = LEDS_GET;
    command.operand_type = OT_NONE;
    command.operand = -1;
    command.parport = 0;
    command.error = "Argument <parport> starts with unexpected character";
    command.rest = s;
    return command;
  }

  /* Выполняем преобразование строки с номером порта в число */
  errno = 0;
  p = s;
  unsigned long parport = strtoul(s, &p, 0);

  /* Анализируем ошибки переполнения */
  if (errno == ERANGE)
  {
    command.command_type = CT_WRONG;
    command.leds_operation = LEDS_GET;
    command.operand_type = OT_NONE;
    command.operand = -1;
    command.parport = 0;
    command.error = "Argument <parport> has too big value";
    command.rest = s;
    return command;
  }
  command.parport = (unsigned)parport;
  s = skip_spaces(p);

  /* Если за номером порта идёт какое-то непотребство, сигнализируем об этом */
  if (s[0] != '\0')
  {
    command.command_type = CT_WRONG;
    command.leds_operation = LEDS_GET;
    command.operand_type = OT_NONE;
    command.operand = -1;
    command.parport = 0;
    command.error = "Unexpected character after <parport> value";
    command.rest = s;
    return command;
  }

  return command;
}

#define IN_BUF_SIZE 32
#define OUT_BUF_SIZE 128

/* Структура данных, содержащая текущее состояние клиента */
struct client_s
{
  int overflow; /* Признак переполнения входящего буфера */
  int exit;     /* Признак того, что клиент запросил команду отключения */

  /* Указатель на каталог портов, которыми управляет клиент */
  parports_t *parports;

  /* Буферы ввода и вывода и количество байтов в них */
  char in_buf[IN_BUF_SIZE + 1];
  size_t in_size;
  char out_buf[OUT_BUF_SIZE + 1];
  size_t out_size;
};

typedef struct client_s client_t;

/* Функция выполнения команды во входном буфере. Должна вызываться тогда,
   когда во входном буфере будет собрана полная строка. Перед вызовом
   функции символ перевода строки должен быть заменён на нулевой байт */
int client_execute_command(client_t *client)
{
  if (client == NULL)
  {
    log_message(LOG_ERR, "client_execute_command: client pointer is NULL");
    return -1;
  }

  /* Если в выходном буфере есть непрочитанный ответ на предыдущую команду,
     то новую команду не выполняем */
  if (client->out_size > 0)
  {
    log_message(LOG_ERR, "client_execute_command: client not yet readed response on previous command");
    return -1;
  }

  /* Если во входном буфере ничего нет, то и команды нет - выполнять нечего */
  if (client->in_size == 0)
  {
    log_message(LOG_ERR, "client_execute_command: empty input");
    return -1;
  }

  /* Анализируем команду во входном буфере */
  command_t command = parse_command(client->in_buf);

  /* Распознана команда чтения или изменения состояния светодиодов на параллельном порту */
  if (command.command_type == CT_LEDS)
  {
    ssize_t size = 0;

    /* Выполняем команду. Если в процессе выполнения произошли ошибки, то сообщаем об этом */
    int leds = parports_leds_ctl(client->parports, command.parport, command.leds_operation, command.operand);
    if (leds == -1)
    {
      log_message(LOG_ERR, "client_execute_command: failed to execute command");
      size = snprintf(client->out_buf, OUT_BUF_SIZE, "Failed to execute command.\n");
    }
    /* Если в процессе выполнения команды ошибок не было, то возвращаем новое состояние светодиодов */
    else
    {
      size = snprintf(client->out_buf, OUT_BUF_SIZE, "0x%04X\n", leds);
    }

    /* Если возникил ошибки при формировании ответа в буфере, то клиенту ответ не возвращаем */
    if (size < 0)
    {
      log_message(LOG_ERR, "client_execute_command: failed to prepare response");
      return -1;
    }

    /* Ответ в буфере сформирован корректно, заполняем поле размера и проставляем завершающий нулевой байт */
    client->out_size = size;
    client->out_buf[client->out_size] = '\0';
  }
  /* Распознана команда отключения клиента от сервера */
  else if (command.command_type == CT_EXIT)
  {
    client->exit = 1;
  }
  /* В процессе анализа команды во входном буфере были найдены ошибки */
  else if (command.command_type == CT_WRONG)
  {
    log_message(LOG_ERR, "client_execute_command: parse_command failed with error '%s', unparsed rest of string - '%s'", command.error, command.rest);

    /* Формируем в буфере ответа сообщение об ошибке */
    ssize_t size = snprintf(client->out_buf, OUT_BUF_SIZE, "%s, unparsed rest of string: %s\n", command.error, command.rest);

    /* Если возникил ошибки при формировании ответа в буфере, то сообщение об ошибке клиенту не возвращаем */
    if (size < 0)
    {
      log_message(LOG_ERR, "client_execute_command: failed to prepare error message");
      return -1;
    }

    /* Сообщение об ошибке в буфере сформировано корректно, заполняем поле размера и проставляем завершающий нулевой байт */
    client->out_size = size;
    client->out_buf[client->out_size] = '\0';
    return -1;
  }

  return 0;
}

/* Функция обрабатывает очередную порцию данных, попавших в буфер чтения. Среди новых данных
   ищется конец строки.

   Если конец строки найден, а флаг переполнения буфера не активен, то вызывается обработка команды.

   Если конец строки найден, а флаг переполнени буфера активен, то в ответ возвращается сообщение
   об ошибке переполнения буфера, а флаг переполнения буфера сбрасывается.

   Если новые данные переполнили буфер, то проставляется флаг переполнения буфера. */
int client_parse_input(client_t *client, size_t offset, ssize_t size)
{
  if (client == NULL)
  {
    log_message(LOG_ERR, "client_parse_input: client pointer is NULL");
    return -1;
  }

  if (size < 0)
  {
    log_message(LOG_ERR, "client_parse_input: negative size of readed data");
    return -1;
  }

  /* Увеличиваем размер данных в буфере чтения на величину новой порции данных */
  client->in_size += size;

  /* Ищем в новых данных символ конца строки */
  for(size_t p = offset; p < client->in_size; p++)
  {
    /* Если найден конец строки, то обрабатываем строку */
    if (client->in_buf[p] == '\n')
    {
      client->in_buf[p] = '\0';

      /* Если не было переполнения буфера ввода, то пытаемся выполнить команду */
      if (client->overflow == 0)
      {
        /* Если выполнение команды не было успешным, то сообщаем об этом в журнал */
        if (client_execute_command(client) == -1)
        { 
          log_message(LOG_WARNING, "client_parse_input: warning, client_execute_command failed");
        }
      }
      else
      {
        /* Сообщаем клиенту о том, что его команда была очень длинной */ 
        client->out_size = snprintf(client->out_buf, OUT_BUF_SIZE, "Too long command was skipped.\n");
        client->out_buf[client->out_size] = '\0';
      }

      /* Т.к. конец строки найден, то буфер ввода (теперь) не переполнен */
      client->overflow = 0;

      /* Возвращаем количество обработанных байт из буфера ввода */
      return p + 1;
    }
  }

  /* Если конец команды не был обнаружен, но буфер полон, то произошло переполнение буфера */
  if (client->in_size == IN_BUF_SIZE)
  {
    client->in_size = 0;
    log_message(LOG_WARNING, "client_parse_input: warning, input buffer overflowed");
    client->overflow = 1;
  }

  /* Не было обработано ни одного байта из буфера ввода */
  return 0;
}

/* Функция-обработчик событий в сокете.

   При поступлении данных в буфер чтения выполняется проверка, получена ли полная команда.
   Если команда получена, она выполняется.

   Если в выходном буфере есть данные, а сокет готов к записи, то данные выводятся в сокет */
int client_process_event(int fd, int events, void *data)
{
  if (data == NULL)
  {
    log_message(LOG_ERR, "client_process_event: data pointer is NULL");
    return -1;
  }

  client_t *client = data;

  ssize_t w = 0; /* Записано байт из буфера вывода */
  ssize_t r = 0; /* Прочитано байт в буфер ввода */
  ssize_t p = 0; /* Обработано байт из буфера ввода */

  /* Если клиент прочитал ответ на предыдущий запрос и отправляет новый запрос,
     то читаем поступающие данные в буфер ввода */
  if ((client->out_size == 0) && (events & EPOLLIN))
  {
    /* Пытаемся прочитать данные в свободную часть буфера */
    r = read(fd, &(client->in_buf[client->in_size]), IN_BUF_SIZE - client->in_size);
    client->in_buf[client->in_size + r] = '\0';

    /* Если что-то прочиталось, то обрабатываем поступившие данные */
    if (r > 0)
    {
      p = client_parse_input(client, client->in_size, r);

      /* Если что-то из данных в буфере ввода было обработано, то удаляем это из буфера */
      if (p > 0)
      {
        memmove(client->in_buf, &(client->in_buf[p]), client->in_size - p);
        client->in_size -= p;
      }
    }
  }

  /* Если в буфере вывода есть данные ответа на прошлый запрос и
     клиент готов принимать данные, то отправляем данные клиенту */
  if ((client->out_size > 0) && (events & EPOLLOUT))
  {
    /* Пытаемся записать всё, что есть в буфере */
    w = write(fd, client->out_buf, client->out_size);

    /* Если что-то записалось, то удаляем это из буфера */
    if (w > 0)
    {
      memmove(client->out_buf, &(client->out_buf[w]), client->out_size - w);
      client->out_size -= w;
    }
  }

  /* Если клиент ввёл команду завершения сеанса, то завершаем работу с клиентом */
  if (client->exit == 1)
  {
    return 0;
  }

  /* Если клиент завис или произошла ошибка, то завершаем работу с клиентом */
  if (events & (EPOLLERR | EPOLLHUP))
  {
    log_message(LOG_ERR, "client_parse_event: connection broken");
    return -1;
  }

  /* Хороший, годный клиент. Обновляем ожидаемые события */

  /* Если есть данные для отправки, то не принимаем от клиента новые команды,
     пока он не прочитает ответ на уже выполенную команду */
  if (client->out_size > 0)
  {
    return EPOLLOUT;
  }

  /* Если данных для отправки, то ожидаем поступления новых команд от клиента */
  return EPOLLIN;
}

/* Освобождение памяти, занятых приватными данными клиента */
int client_destroy(void *data)
{
  if (data == NULL)
  {
    log_message(LOG_ERR, "client_destroy: data is NULL pointer");
    return -1;
  }

  free(data);
  return 0;
}

/* Создание клиента, управляющего светодиодами на параллельных портах */
socket_t *client_create(int fd, parports_t *parports)
{
  if (parports == NULL)
  {
    log_message(LOG_ERR, "client_create: parports is NULL pointer");
    return NULL;
  }

  /* Выделяем память под хранение приватных данных клиента */
  client_t *client = malloc(sizeof(client_t));
  if (client == NULL)
  {
    log_message(LOG_ERR, "client_create: failed to allocate memory for client");
    return NULL;
  }
  client->overflow = 0;
  client->exit = 0;
  client->parports = parports;
  client->in_size = 0;
  client->out_size = 0;

  /* Создание нового сокета для помещения в цикл ожидания событий */
  socket_t *socket = socket_create(fd, EPOLLIN, client_process_event, client_destroy, client);
  if (socket == NULL)
  {
    log_message(LOG_ERR, "client_create: socket_create failed");
    free(client);
    return NULL;
  }

  return socket;
}
