#ifndef __CLIENT__
#define __CLIENT__

#include "parports.h"

/* Создание клиента, управляющего светодиодами на параллельных портах */
socket_t *client_create(int fd, parports_t *parports);

#endif
