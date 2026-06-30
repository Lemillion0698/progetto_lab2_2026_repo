#include "coda.h"

int queue_init(queue_t* q, size_t capacità){
    if(q == NULL){
        return QUEUE_CLOSE;
    }
    // Allocazione sullo heap di uno spazio per accogliere un array "circolare"
    q->elem = malloc(capacità*sizeof(void*));
    if ((q->elem) == NULL) {
        return QUEUE_CLOSE;
    }

    // Inizializzazione di campi
    q->capacity = capacità;
    q->head = 0;
    q->tail = 0;
    q->count = 0; // non ci sono ancora elementi presenti 
    q->closed = 0;

    mtx_init(&q->mutex, mtx_plain);
    cnd_init(&q->not_empty);
    cnd_init(&q->not_full);

    return QUEUE_OK;
}

int queue_push(queue_t* q, void* elem){ // Inserisco una riga nell'array
    if(q == NULL){
        return QUEUE_CLOSE;
    }
    mtx_lock(&(q->mutex)); // acquisisco il mutex

    if(q->closed == 1){ // se la coda è chiusa, non inserisco più
        mtx_unlock(&(q->mutex));
        return QUEUE_CLOSE;
    }

    while (q->count == q->capacity){ // Se la coda è piena, aspetta
        if (q->closed){
            mtx_unlock(&(q->mutex));
            return QUEUE_CLOSE;
        }
        cnd_wait(&(q->not_full), &(q->mutex));
    }

    q->elem[q->tail] = elem; // Inserisco in coda

    q->tail = (q->tail + 1) % q->capacity; // aggiorno tail in modo circolare
    q->count++; // il numero di elementi è aumentato

    cnd_broadcast(&(q->not_empty)); // un elemento è stato aggiunto
    mtx_unlock(&(q->mutex)); // rilascio il mutex

    return QUEUE_OK;
}

int queue_pop(queue_t* q, void** elem){
    if(q == NULL){
        return QUEUE_CLOSE;
    }
    mtx_lock(&(q->mutex));

    while(q->count == 0 && q->closed == 0){ // finché la coda è vuota e NON chiusa...
        cnd_wait(&(q->not_empty), &(q->mutex)); // ... tutti i thread consumatori aspetano passivamente : in sospeso, senza consumare CPU
    }

    if(q->count == 0 && q->closed == 1){ // se la coda è chiusa, non estrarre più niente
        mtx_unlock(&(q->mutex)); // bisogna rilascia il mutex per evitare una deadlock
        return QUEUE_CLOSE;
    }
    // estraggo un elemento della coda
    *elem = q->elem[q->head];

    q->head = (q->head + 1) % q->capacity; // aggiorno l'indice di estrazione
    q->count--;

    cnd_broadcast(&(q->not_full)); // un elemento è stato rimosso
    mtx_unlock(&(q->mutex)); // rilascio il mutex


    return QUEUE_OK;
}

int queue_close(queue_t* q){
    if(q == NULL){
        return QUEUE_CLOSE;
    }
    mtx_lock(&(q->mutex)); // acquisisco il mutex
    q->closed = 1; // chiudo la coda: non sono più possibili inserimenti dal reader
    
    cnd_broadcast(&(q->not_full));
    cnd_broadcast(&(q->not_empty));
    mtx_unlock(&(q->mutex)); // rilascio il mutex che è stato acquisito

    return QUEUE_OK;
}

int queue_destroy(queue_t* q){
    if(q == NULL){
        return QUEUE_CLOSE;
    }

    // distruggo mutex e conditions variables
    mtx_destroy(&(q->mutex));
    cnd_destroy(&(q->not_empty));
    cnd_destroy(&(q->not_full));

    // libero q->elem, cioè l'array circolare
    free(q->elem);

    return QUEUE_OK;
}