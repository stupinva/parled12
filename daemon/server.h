#ifndef __SERVER__
#define __SERVER__

#include "parports.h"
#include "evloop.h"

/* Создание сервера для обслуживания клиентов, управляющих светодиодами на параллельных портах */
socket_t *server_create(int fd, evloop_t *evloop, parports_t *parports);

#endif
