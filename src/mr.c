#include <stdlib.h>
#include "../include/mr.h" // path relativo

#define MR_DEFAULT_MAPPER_THREADS 1
#define MR_DEFAULT_REDUCER_THREADS 1
#define MR_DEFAULT_QUEUE_SIZE 100
#define MR_DEFAULT_LOG_FILE "mr.log" // Nome di default del file di log

struct mr {
    size_t mapper_threads;
    size_t reducer_threads;
    size_t queue_size;
    const char *log_file;

    mr_hash_t hash;
    void *hash_arg;

    // servono per la funzione mr_create()
    mr_mapper_t mapper;
    mr_reducer_t reducer;
    void *user_arg;
};

int mr_attr_init(mr_attr_t* attr){
    if (attr == NULL) {
        return -1;
    }
    attr->mapper_threads = MR_DEFAULT_MAPPER_THREADS; // per default, un solo thread nel processo Mapper
    attr->reducer_threads = MR_DEFAULT_REDUCER_THREADS;// per default, un solo thread nel processo Reducer

    attr->queue_size = MR_DEFAULT_QUEUE_SIZE; // è la capacità massima della coda C11 interna ai processi (il tuo circular buffer)

    attr->log_file = MR_DEFAULT_LOG_FILE;

    attr->hash = NULL;
    attr->hash_arg = NULL;
    
    return 0; // tutto è andata bene: ha impostato tutti i campi di mr_attr_t con valori di default

}

int mr_attr_destroy(mr_attr_t *attr) {
    if (attr == NULL) 
        return -1;
    // Non è stato allocato dinamicamente nulla con attr, allora non serve free()
    /*non ci sono risorse da rilasciare, la funzione è essenzialmente un no-op con solo il check NULL.*/
    return 0;
}

int mr_attr_set_mapper_threads(mr_attr_t* attr, size_t n){
    if(attr == NULL || n==0){ // non posso avere 0 thread dentro il processo Mapper
        return -1;
    }
    attr->mapper_threads = n;
    return 0;
}

int mr_attr_set_reducer_threads(mr_attr_t* attr, size_t n){
    if(attr == NULL || n==0){ // non posso avere 0 thread dentro il processo Reducer
        return -1;
    }
    attr->reducer_threads = n;
    return 0;
}

int mr_attr_set_queue_size(mr_attr_t *attr, size_t n){
    if(attr == NULL || n==0){ // non posso avere una coda interna ad un processo vuota
        return -1;
    }
    attr->queue_size = n;
    return 0;
} 

int mr_attr_set_log_file(mr_attr_t *attr, const char *path){
    if ((attr == NULL) || (path == NULL)){
        return -1;
    }
    attr->log_file =  path;
    return 0;
}

int mr_attr_set_hash_function(mr_attr_t *attr, mr_hash_t hash, void* hash_arg){
    if ((attr == NULL) || (hash == NULL)){
        return -1;
    }
    // setto i campi della struct mr_attr_t, corrispondenti alla funzione di hashing
    attr->hash = hash;
    attr->hash_arg = hash_arg; 

    return 0;
}


int mr_create(mr_t *mr, const mr_attr_t* attr, mr_mapper_t mapper, mr_reducer_t reducer, void *user_arg){ // crea un nuovo oggetto di tipo "struct mr" che rapprenta una singola una singola esecuzione MapReduce
    // Controllo dei parametri
    if(mr == NULL || attr == NULL) {
        return -1;
    }

    // Allocazione sullo heap di una nuova istanza del framework, un nuovo oggetto di tipo mr_t simplicemente
    if(((*mr) = malloc(sizeof(struct mr))) == NULL) {
        return -1;
    } 

    // inizializzazione dei campi opportuni di mr
    (*mr)->mapper_threads = attr->mapper_threads;
    (*mr)->reducer_threads = attr->reducer_threads;
    (*mr)->queue_size = attr-> queue_size;
    (*mr)->log_file = attr->log_file;
    (*mr)->hash = attr->hash;
    (*mr)->hash_arg = attr->hash_arg;
    (*mr)->mapper = mapper; // passo la funzione mapper definita dall'utente
    (*mr)->reducer = reducer; // passo la funzione reducer definita dall'utente
    (*mr)->user_arg = user_arg;

    return 0;
}

int mr_destroy(mr_t mr){
    if(mr == NULL){
        return -1;
    }
    free(mr);
    return 0;
}