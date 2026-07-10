#include <unistd.h>
#include <stdio.h>
#include <threads.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include "log.h"

int log_write(const char* log_path, const char* sem_name, const char* evento, const char* messaggio){
    if(!log_path || !sem_name ){
        fprintf(stderr, "file di log non valido o semaforo non esistente\n");
        return -1;
    }

    // Apertura del semaforo
    sem_t* semaforo = sem_open(sem_name, 0); // 0: flag per aprire un semaforo già esistente
    if(semaforo == SEM_FAILED){
        fprintf(stderr, "Semaforo non aperto con successo\n");
        return -1;
    }

    // Acquisisco il semaforo
    sem_wait(semaforo);

    // Apertura del file di log in APPEND
    FILE* fp = fopen(log_path, "a");
    if(fp == NULL){ sem_post(semaforo); sem_close(semaforo); return -1; }

    // Scrivo una riga nel file di log
    char timestamp[64];
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    
    long int pid = (long int)getpid();
    long int tid = (long int)thrd_current();
    
    fprintf(fp, "[%s] [%ld] [%ld] [%s] %s\n", timestamp, pid, tid, evento, messaggio);

    // Chiudo il file
    fclose(fp);

    // Rilascio il semaforo
    sem_post(semaforo);

    // Chiudo il gestore del semaforo
    sem_close(semaforo);

    return 0;
}
