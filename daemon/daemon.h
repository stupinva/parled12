#ifndef __DAEMON__
#define __DAEMON__

#include <errno.h>
#include <syslog.h>

/* Переход в режим демона:
   1. переход в корневой каталог,
   2. ветвление и завершение родительского процесса,
   3. открытие новой сессии,
   4. связывание потоков стандартного ввода-вывода с пустым устройством */
int daemonize();

/* Функция для отправки сообщений в журнал. Напрямую не вызывается,
   используется только в макросах log_error и log_message */
void logger(int priority, int err, const char *filename, const int line, const char *format, ...);

/* Макросы log_error и log_message выполнены в виде макросов для того, чтобы вызывающая сторона
   могла не указывать функции logger имя файла с исходным текстом и номер строки, из которой
   была вызвана функция. Соответствующие данные берутся из макросов __FILE__ и __LINE__, которые
   определены препроцессором в каждой из обрабатываемых им строк исходного текста.

   Если в текст сообщения нужно добавить сообщение об ошибке, соответствующее текущему значению
   переменной errno, то нужно использовать макрос log_error. Если же этого делать не нужно, то
   следует использовать макроc log_message. При этом, любому из этих макросов можно указать
   произвольный уровень важности сообщения.

   Оба макроса используются абсолютно аналогично: в первом аргументе необходимо указать
   уровень важности из syslog.h, во втором аргументе указывается форматная строка, как в
   функции printf, а далее следуют аргументы, значения которых используются в форматной строке */
#define log_error(priority, ...) logger(priority, errno, __FILE__, __LINE__, __VA_ARGS__)
#define log_message(priority, ...) logger(priority, 0, __FILE__, __LINE__, __VA_ARGS__)

#endif