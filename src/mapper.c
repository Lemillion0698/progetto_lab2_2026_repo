#include <stdio.h>
#include <stdlib.h>
#include "mapper.h"
#include "io.h"
#include "../include/mr.h"
#include "mr_internal.h"


int emit_pair(const char*, const void*, size_t, void*); // dichiarazione forward di emit_pair

static int reader_main(void* arg){ // funzione del thread lettore
    // Cast dell'argomento a reader_arg_t
    reader_arg_t* a = (reader_arg_t*)arg;

    // Leggere i messaggi da stdin in loop
    
    ssize_t r1;
    
    size_t file_name_len_tmp;
    while((r1 = readn(STDIN_FILENO, &file_name_len_tmp, sizeof(file_name_len_tmp))) > 0){   
        mr_file_line_t* linea = malloc(sizeof(mr_file_line_t));
        if (!linea){
            return QUEUE_CLOSE;
        }
        linea->file_name_len = file_name_len_tmp;

        // leggi line_number, line_len
        ssize_t r2 = readn(STDIN_FILENO, &(linea->line_number), sizeof(linea->line_number));
        controllo_io(r2);
        ssize_t r3 = readn(STDIN_FILENO, &(linea->line_len), sizeof(linea->line_len));
        controllo_io(r3);

        // alloca buffer, leggi payload
        char* punt_buffer_file_name = malloc(linea->file_name_len + 1);
        if(!punt_buffer_file_name){ 
            free(linea);
            return QUEUE_CLOSE;
        }
        char* punt_buffer_line = malloc (linea->line_len + 1);
        if(!punt_buffer_line){
            free(linea);
            free(punt_buffer_file_name);
            return QUEUE_CLOSE;
        }
        ssize_t r4 = readn(STDIN_FILENO, punt_buffer_file_name, linea->file_name_len);
        controllo_io(r4);
        ssize_t r5 = readn(STDIN_FILENO, punt_buffer_line, linea->line_len);
        controllo_io(r5);

        // aggiungo i terminatori
        punt_buffer_file_name[linea->file_name_len] = '\0';
        punt_buffer_line[linea->line_len] = '\0';
        // assegna a linea
        linea->file_name = punt_buffer_file_name;
        linea->line = punt_buffer_line;

        // Inserisco ogni linea nella coda
        queue_push(a->coda, linea);

    }

    // Quando EOF chiamare queue_close()
    queue_close(a->coda);
    
    return QUEUE_OK;
}

static int mapper_worker_main(void* arg){
    worker_arg_t* a = (worker_arg_t*)arg; // cast esplicito del parametro

    // ogni volta che un worker estrae una riga dalla coda, invoca la mapper fornita dall'utente
    void* item; // serve di deposito all'elemento estratto
    
    
    while(queue_pop(a->coda, &item) == QUEUE_OK){
        mr_file_line_t* linea = (mr_file_line_t*)item;
        
        mtx_lock((a->emit_mutex));
        a->accesso->mapper(linea, a->emit_fn, NULL, a->accesso->user_arg);
        mtx_unlock((a->emit_mutex));

        free((char*)linea->file_name);
        free((char*)linea->line);
        free(linea);
    }
    

    return QUEUE_OK;
}


int mapper_run(mr_t mr){
    // inizializza coda, mutex
    queue_t* coda = malloc(sizeof(queue_t));
    if (!coda){
        return QUEUE_CLOSE;
    }
    queue_init(coda, mr->queue_size); 

    mtx_t emit_mutex;
    mtx_init(&emit_mutex, mtx_plain);

    // lancia reader + worker threads
    thrd_t lettore;
    reader_arg_t arg_lettore;
    arg_lettore.coda = coda;
    arg_lettore.fd = STDIN_FILENO;
    if(thrd_create(&lettore, reader_main, &arg_lettore) != thrd_success){
        return -1;
    }

    thrd_t worker[mr->mapper_threads];
    worker_arg_t arg_worker[mr->mapper_threads];
    for(size_t i = 0; i < mr->mapper_threads; i++){
        arg_worker[i].coda = coda;
        arg_worker[i].accesso = mr;
        arg_worker[i].emit_mutex = &emit_mutex;
        arg_worker[i].emit_fn = emit_pair;

        if(thrd_create(&worker[i], mapper_worker_main, &arg_worker[i]) != thrd_success){
            return -1;
        }
    }

    // join
    if (thrd_join(lettore, NULL) != thrd_success){
        fprintf(stderr, "Thread lettore non terminato correttamente\n");
        return -1;
    }

    for(size_t i=0; i<mr->mapper_threads; i++){
        if (thrd_join(worker[i], NULL) != thrd_success){
            fprintf(stderr, "Thread worker %zu non terminato correttamente\n",i);
            return -1;
        }
    }
    // distruggi
    mtx_destroy(&emit_mutex);
    queue_destroy(coda);
    free(coda);

    return 0;
}
