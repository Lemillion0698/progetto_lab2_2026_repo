#ifndef CODA_H
#define CODA_H

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>   /* C11: mtx_t, cnd_t, thrd_t, thrd_create... */

// Valori di ritorno della funzioni di utilità
#define QUEUE_OK 0
#define QUEUE_CLOSE (-1)

// Un elemento della coda
typedef struct {
    void** elem;    /* buffer circolare di puntatori */
    size_t  capacity; /* capacità massima */
    size_t  head;     /* indice estrazione */
    size_t  tail;     /* indice inserimento */
    size_t  count;    /* elementi presenti */
    int     closed;   /* 1 dopo queue_close(), 0 altrimenti */
    mtx_t   mutex;
    cnd_t   not_empty; /* segnalata quando un elemento viene aggiunto */
    cnd_t   not_full;  /* segnalata quando un elemento viene rimosso */
} queue_t;

// Funzioni per gestire la coda
int queue_init(queue_t *q, size_t capacity);
int queue_push(queue_t *q, void *item);
int queue_pop(queue_t *q, void **item);
int queue_close(queue_t *q);
int queue_destroy(queue_t *q);

#endif