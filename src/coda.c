#include "coda.h"
#include "io.h"
#include "../include/mr.h"

int reader_main(void* arg){ // funzione del thread lettore
    // Cast dell'argomento a reader_arg_t
    reader_arg_t* a = (reader_arg_t*)arg;

    // Leggere i messaggi da stdin in loop
    ssize_t r1;
    while(r1 = read(STDIN_FILENO, ))
    // Inserisco la linea nella coda
    queue_push();

    // Quando EOF chiamare queue_close()
    if(r1 == EOF){
        queue_close(arg->queue);
    }

    return QUEUE_OK;
}