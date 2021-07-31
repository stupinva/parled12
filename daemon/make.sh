#!/bin/sh

gcc -std=c99 -Wpedantic -Wall -Wextra -D_DEFAULT_SOURCE -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--print-gc-sections -Wl,-s -o parled12 daemon.c parport.c parports.c evloop.c client.c server.c slave.c config.c master.c main.c
gcc -std=c99 -Wpedantic -Wall -Wextra -D_DEFAULT_SOURCE -DLITE -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--print-gc-sections -Wl,-s -o parled12-lite daemon.c parport.c parports.c evloop.c client.c server.c slave.c config.c main.c
