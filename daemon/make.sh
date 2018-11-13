#!/bin/sh

gcc -std=c99 -Wpedantic -Wall -Wextra -D_DEFAULT_SOURCE -o parled12 daemon.c parport.c parports.c evloop.c client.c server.c slave.c config.c master.c main.c
gcc -std=c99 -Wpedantic -Wall -Wextra -D_DEFAULT_SOURCE -DLITE -o parled12-lite daemon.c parport.c parports.c evloop.c client.c server.c slave.c config.c main.c
#gcc -std=c99 -Wpedantic -Wall -Wextra -o parled12 parport.c parports.c
