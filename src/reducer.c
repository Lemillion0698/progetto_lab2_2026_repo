#include <stdio.h>
#include <stdlib.h>
#include "io.h"
#include "reducer.h"
#include "../include/mr.h"

/*
reader_main ha due responsabilità:
1) Riempire la hash table — legge tutte le coppie <token, val> dalla pipe B e le inserisce nella hash table tramite hashtable_insert.
2) Segnalare ai worker — quando riceve EOF (il mapper ha chiuso la pipe B), imposta il flag e fa cnd_broadcast per svegliare tutti i worker che 
   stavano aspettando.
*/

static int reader_main(void* arg){ // funzione del thread lettore
    // Cast dell'argomento
    arg_lettore* a = (arg_lettore* )arg;
    
    // contenitori degli header
    size_t token_len;
    size_t val_len;

    while(readn(a->fd, &token_len, sizeof(token_len)) > 0){
        ssize_t r2 = readn(a->fd, &val_len, sizeof(val_len));
        controllo_io(r2);

        // Alloco e leggo token
        char* token = malloc(token_len+1); 
        controllo_io(readn(a->fd, token, token_len));
        token[token_len] = '\0';

        // Alloco e leggo val
        void* val = malloc(val_len);
        controllo_io(readn(a->fd, val, val_len));

        hashtable_insert(a->ht, token, val, val_len);

        // libero token e val
        free(token);
        free(val);

    }

    // imposto flag e chiamo broadcast
    mtx_lock(a->emit_mutex);
    *(a->flag) = 1;
    cnd_broadcast(a->cd_var);
    mtx_unlock(a->emit_mutex);

    return 0;
}

/* worker_main ha un solo ruolo: processare le entry della hash table.
Per ogni entry che gli viene assegnata:

1. Raccoglie tutti i nodo della lista valori → costruisce un array di valori
2. Invoca la callback reducer(token, valori[], n_valori, emit_result, user_arg) dell'utente
3. Continua finché non ci sono più entry da processare */
static int worker_main(void* arg){ // funzione per i threads worker
    arg_worker* a = (arg_worker* )arg; // cast del parametro
    mtx_lock(a->emit_mutex);
    while(*(a->flag) == 0) // per prevenire i risvegli spuri
        cnd_wait(a->cd_var, a->emit_mutex); // aspetta che il lettore finisca
    mtx_unlock(a->emit_mutex); 
    
    size_t nuovo_indice = *(a->shared_index); // prendo il prx indice da processare

    entry_hash_table* punt_entry = a->ht->arr_entry[nuovo_indice]; // puntatore al bucket corrente
    
    int i=0;
    while(punt_entry != NULL){
        nodo* punt_testa = punt_entry->testa;
        nodo* curr = punt_testa; // per contare il numero di nodi di una entry_hash_table
        int num_nodi=0;
        while(curr != NULL){
            num_nodi++;
            curr = curr->next;
        }
        void** arr_valori = malloc(num_nodi*sizeof(void*)); // un array per i valori opachi
        for( int i=0; i<num_nodi; i++){
            arr_valori[i] = a->ht->arr_entry[nuovo_indice]->testa->punt_byte;
        }
        reducer(token, arr_valori, num_nodi, emit_result, user_arg);
        free(arr_valori);
        punt_entry= a->ht->arr_entry[nuovo_indice++]; // vado al prossimo bucket
    }

    return 0;
}