#include <stdio.h>
#include <stdlib.h>
#include "mapper.h"
#include "io.h"

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

    return QUEUE_OK;
}