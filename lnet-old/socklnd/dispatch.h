#ifndef DISPATCH_H
#define DISPATCH_H

#include <socklnd/pqtimer.h>

/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (c) 2002 Cray Inc.
 *  Copyright (c) 2002 Eric Hoffman
 *
 *   This file is part of Portals, http://www.sf.net/projects/sandiaportals/
 */

/* this file is only called dispatch.h to prevent it
   from colliding with /usr/include/sys/select.h */

typedef struct io_handler *io_handler;

struct io_handler{
        io_handler *last;
        io_handler next;
        int fd;
        int type;
        int (*function)(void *);
        void (*errf)(void *);
        void *argument;
        int disabled;
};


#define READ_HANDLER 1
#define WRITE_HANDLER 2
#define EXCEPTION_HANDLER 4
#define ALL_HANDLER (READ_HANDLER | WRITE_HANDLER | EXCEPTION_HANDLER)

io_handler register_io_handler(int fd,
                               int type,
                               io_handler *handler_head,
                               int (*function)(void *),
			       void (*errf)(void *),
                               void *arg);

void remove_io_handler (io_handler i);
void init_unix_timer(void);
void select_timer_block(when until, void **arg);
when now(void);

/*
 * hacking for CFS internal MPI testing
 */ 
#define ENABLE_SELECT_DISPATCH
#endif
