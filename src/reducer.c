#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// dichiarazione forward di emit_result
static int emit_result(const char* token, const void* result, size_t result_size, void* emit_arg); 

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
    arg_worker* a = (arg_worker*)arg; // cast del parametro
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
           mr_value_t* arr_valori = malloc(num_nodi*sizeof(mr_value_t)); // per raccogliere tutti i valori dei nodi
            if(arr_valori == NULL){
                fprintf(stderr, "arr_valori non allocato con successo\n");
                return -1;
            }
            // 3. riempilo
            for(size_t i=0; i<num_nodi; i++){
                arr_valori[i].data = punt_testa->punt_byte;
                arr_valori[i].size = punt_testa->lunghezza;
                punt_testa = punt_testa->next;
            }
            // 4. chiama a->accesso->reducer(...)
            a->accesso->reducer(entry->token, arr_valori, num_nodi, a->rs_finale, NULL, a->accesso->user_arg);

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

    // Costruzione di arg_lettore
    arg_lettore let;
    let.fd = STDIN_FILENO;
    let.ht = &ht;
    let.emit_mutex = &mutex;
    let.cd_var = &cv;
    let.flag = &flag;
    // Creo il thread lettore
    thrd_t lettore;
    if(thrd_create(&lettore, reader_main, &let) != thrd_success){
        fprintf(stderr, "lettore non creato con successo\n");
        return -1;
    }


    // Costruzione di arg_worker[]
    size_t num_workers = mr->reducer_threads;
    arg_worker wk[num_workers];
    for(size_t i=0; i<num_workers; i++){
        wk[i].accesso = mr;
        wk[i].cd_var = &cv;
        wk[i].emit_mutex = &mutex;
        wk[i].flag = &flag;
        wk[i].ht = &ht;
        wk[i].shared_index = &shared_index;
        wk[i].rs_finale = emit_result;
    }
    // creo i threads workers
    thrd_t worker[num_workers];
    for(size_t i=0; i<num_workers; i++){
        if(thrd_create(&worker[i], worker_main, &wk[i]) != thrd_success){
            fprintf(stderr, "worker[%zu] non creato con successo\n", i);
            return -1;
        }
    }

    // Terminazione dei threads
    if(thrd_join(lettore, NULL) != thrd_success){
        fprintf(stderr, "lettore non unito con successo\n");
        return -1;
    }
    for(size_t i=0; i<num_workers; i++){
        if(thrd_join(worker[i], NULL) != thrd_success){
            fprintf(stderr, "worker[%zu] non unito con successo\n", i);
            return -1;
        }
    }

    // Distruzione dei meccanismi di sincronizzazione e della tabella hash
    hashtable_destroy(&ht);
    mtx_destroy(&mutex);
    cnd_destroy(&cv);

    return 0;
}

static int emit_result(const char* token, const void* result, size_t result_size, void* emit_arg){
    // soppressione di warning
    (void)emit_arg;

    // Controllo sulla validità degli elementi della coppia da emettere
    if(token == NULL || result == NULL){
        return -1;
    }

    // Serializzo i dati da mandare sulla pipe C

    //Mando prima gli header...
    size_t token_len = strlen(token) + 1;
    ssize_t w1 = writen(STDOUT_FILENO, &token_len, sizeof(token_len));
    controllo_io(w1);
    ssize_t w2 = writen(STDOUT_FILENO, &result_size, sizeof(result_size));
    controllo_io(w2);
    // ...poi i payload
    ssize_t w3 = writen(STDOUT_FILENO, token, token_len);
    controllo_io(w3);
    ssize_t w4 = writen(STDOUT_FILENO, result, result_size);
    controllo_io(w4);

    return 0;
}
