#ifndef __PARPORTS__
#define __PARPORTS__

#include "parport.h"

/* Каталог портов */
struct parports_s;
typedef struct parports_s parports_t;

/* Создание каталога портов */
parports_t *parports_create();

/* Добавление в каталог нового порта */
int parports_add(parports_t *parports, const char *pathname);

/* Возвращает количество портов в каталоге */
int parports_number(parports_t *parports);

/* Открыть все порты в каталоге.

   Эта операция вынесена в отдельную функцию для того, чтобы отделить
   момент подготовки списка портов от их открытия и использования.

   Подготовка списка осуществляется ведущим процессом до демонизации,
   открытие портов осуществляется ведомым процессом до сброса привилегий,
   использование портов осуществляется ведомым процессом после сброса привилегий. */
int parports_open(parports_t *parports);

/* Закрыть все порты в таблице, удалить каталог портов */
int parports_destroy(parports_t *parports);

/* Выполнить указанную операцию над портом из каталога.

   Для указания конкретного порта используется его порядковый номер в каталоге.
   Нумерация портов начинается с нуля.

   Операции над портами leds_operation_t определены в parport.h */
int parports_leds_ctl(parports_t *parports, const unsigned parport, leds_operation_t operation, int value);

#endif
