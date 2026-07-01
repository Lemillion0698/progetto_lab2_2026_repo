#ifndef MAPPER_H
#define MAPPER_H

#include "coda.h"
#include "../include/mr.h"

typedef struct { // serve al thread lettore ...
    queue_t *coda; // ...per inserire righe in coda ...
    int fd; // ...per ricevere come input lo stdin del Mapper
} reader_arg_t;

typedef struct { // serve ad un thread worker ...
    queue_t* coda; // la coda da cui estrarre
    mr_t accesso; // per mapper e user_arg della struct mr
    mtx_t *emit_mutex; // per proteggere una coppia mandata sulla pipe B
    mr_emit_pair_t emit_fn;
} worker_arg_t;


int mapper_run(mr_t mr);
#endif
