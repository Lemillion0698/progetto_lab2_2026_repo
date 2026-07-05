#include <stdio.h>
#include <stdlib.h>
#include "io.h"
#include "reducer.h"
#include "../include/mr.h"
#include"mr_internal.h"

#define dim_table 64

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
    
    
    while(1) {
        mtx_lock(a->emit_mutex);
        if(*(a->shared_index) >= a->ht->dim_tabella) {
            mtx_unlock(a->emit_mutex);
            break;
        }
        size_t my_idx = (*(a->shared_index))++;
        mtx_unlock(a->emit_mutex);

        entry_hash_table* entry = a->ht->arr_entry[my_idx];
        while(entry != NULL) {     // loop sulle entry del bucket
            // 1. conta i nodi di entry->testa
            nodo* curr = entry->testa; // per contare i nodi della entry corrente
            nodo* punt_testa = curr; // per raccogliere tutti i valori
            size_t num_nodi = 0;
            while(curr  != NULL){
                num_nodi++;
                curr = curr->next;
            }
            // 2. alloca arr_valori
            void** arr_valori = malloc(num_nodi*sizeof(void*)); // per raccogliere tutti i valori dei nodi
            if(arr_valori == NULL){
                fprintf(stderr, "arr_valori non allocato con successo\n");
                return -1;
            }
            // 3. riempilo
            for(size_t i=0; i<num_nodi; i++){
                arr_valori[i] = punt_testa->punt_byte;
                punt_testa = punt_testa->next;
            }
            // 4. chiama a->accesso->reducer(...)
            a->accesso->reducer(entry->token, arr_valori, num_nodi, a->rs_finale, a->accesso->user_arg);

            // 5. free(arr_valori)
            free(arr_valori);

            entry = entry->next_entry;
        }
    }
    return 0;
}

int reducer_run(mr_t mr){
    if(!mr){
        fprintf(stderr, "puntatore mr non inizializzato in Reducer");
        return -1;
    }

    // Inizializzare hash table, mutex, condition variable, flag, shared_index
    hash_table ht;
    mtx_t mutex;
    cnd_t cv;
    int flag = 0;
    size_t shared_index = 0;

    hashtable_init(&ht, dim_table); // 64 bucket, valore ragionevole

    mtx_init(&mutex, mtx_plain);
    cnd_init(&cv);




    return 0;
}

