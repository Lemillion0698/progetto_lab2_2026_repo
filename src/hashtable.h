#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdlib.h>

typedef struct nodo {
    void* punt_byte;       // i campi di valore_opaco
    size_t lunghezza;      // sono dentro nodo direttamente
    struct nodo* next;
} nodo;

typedef struct entry_hash_table { // Per ogni cella della tabella hash
    char* token; // stringa del token
    nodo* testa; // puntatore alla testa della lista valori
    struct entry_hash_table* next_entry; // puntatore al prx nodo della catena
} entry_hash_table;

typedef struct {
    struct entry_hash_table** arr_entry;
    size_t dim_tabella;
} hash_table;

// funzioni di hashing
int hashtable_init(hash_table* ,size_t);
int hashtable_insert(hash_table* ,char* ,void* ,size_t);
int hashtable_destroy(hash_table* );

#endif