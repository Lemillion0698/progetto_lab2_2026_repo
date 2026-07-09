#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/mr.h"


int mapper(const mr_file_line_t* linea, mr_emit_pair_t emit, void* emit_arg, void* user_arg){
    (void)user_arg; // serve a silenziare il warning

    if(!linea){
        fprintf(stderr, "linea di testo non valida\n");
        return -1;
    }
    
    char* copia_linea = strndup(linea->line, linea->line_len); // faccio una copia della linea perché il campo line è costante
    
    size_t uno = 1;
    char* parola = strtok(copia_linea, " \n\t");
    while(parola != NULL) {
        emit(parola, &uno, sizeof(size_t), emit_arg);
        parola = strtok(NULL, " \n\t");
    }

    free(copia_linea);
    return 0;
}


int reducer (const char *token, const mr_value_t *values, size_t values_count, mr_emit_result_t emit, void *emit_arg, void *user_arg){
    (void)user_arg;

    if (!token){
        fprintf(stderr, "token inserito, non valido\n");
        return -1;
    }
    size_t somma = 0;
    for(size_t i=0; i<values_count; i++){
        somma += *(size_t*)values[i].data;
    }
    emit(token, &somma, sizeof(size_t), emit_arg);

    return 0;
}

int main(void) {
    mr_attr_t attr;
    mr_attr_init(&attr);

    mr_t mr;
    mr_create(&mr, &attr, mapper, reducer, NULL);

    mr_start(mr, "tests/input.txt", "output");

    mr_destroy(mr);
    return 0;
}