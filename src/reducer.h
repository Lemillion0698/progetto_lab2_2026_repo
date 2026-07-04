#ifndef RD_H
#define RD_H

#include "hashtable.h"
#include "../include/mr.h"
#include <threads.h>

typedef struct arg_lettore{
    int fd; 
    hash_table* ht;
    mtx_t* emit_mutex;
    cnd_t* cd_var;
    int* flag;
} arg_lettore;

typedef struct arg_worker{
    hash_table* ht; // un worker deve poter accedere a tutti bucket della tabella hash
    mr_t accesso; // accedo alla callback e al user_arg
    mtx_t* emit_mutex; // per evitare race condition su un bucket
    size_t* shared_index; // punta al prossimo bucket da processare
    cnd_t* cd_var; // per svegliare tutti i worker
    int* flag; // indica che il lettore ha finito
    mr_emit_result_t rs_finale;
} arg_worker;

int reducer_run(mr_t mr);

#endif