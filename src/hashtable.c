#include <stdio.h>
#include <string.h>
#include "hashtable.h"


int hashtable_init(hash_table* ht,size_t dim_tabella){
    if(!ht){
        fprintf(stderr, "tabella hash non presente in memoria");
        return -1;
    }

    // allocazione ed inizializzazione della tabella hash
    if ((ht->arr_entry = calloc(dim_tabella, sizeof(entry_hash_table*))) == NULL){
        fprintf(stderr, "tabella hash non allocata con successo");
        
        return -1;
    }
    ht->dim_tabella = dim_tabella;

    return 0;
}
static size_t hash(const char* token, size_t num_bucket) {
    size_t h = 5381;
    while (*token) h = h * 33 + (unsigned char)*token++;
    return h % num_bucket;
}


static int inserisci_valore_opaco_nel_nodo(entry_hash_table* entry, void* val, size_t lunghezza) {
    // Controllo preventivo sui parametri
    if (entry == NULL || val == NULL) {
        return -1; 
    }

    // 1. Alloco la memoria per la struttura del nuovo nodo
    nodo* nuovo_nodo = (nodo*)malloc(sizeof(nodo));
    if (nuovo_nodo == NULL) {
        return -1; // Errore: memoria esaurita
    }
    
    // copia del valore opaco del nodo appena allocato
    nuovo_nodo->lunghezza = lunghezza;
    if(!(nuovo_nodo->punt_byte = malloc(lunghezza))){
        fprintf(stderr, "spazio per ricevere i byte non avvenuto con successo\n");
        free(nuovo_nodo);
        return -1;
    }
    
    memcpy(nuovo_nodo->punt_byte, val, lunghezza); // copio dalla memoria, il valore opaco


    // 3. Inserimento in testa alla lista concatenata dei valori
    nuovo_nodo->next = entry->testa;  // aggancia la lista esistente
    entry->testa = nuovo_nodo;        // diventa il nuovo primo nodo

    return 0; // Inserimento avvenuto con successo
}

static int hash_table_insert_head(hash_table* ht, size_t index, const char* token) {
    // Controllo di sicurezza sui parametri passati
    if (ht == NULL || ht->arr_entry == NULL || index >= ht->dim_tabella) {
        return -1; 
    }

    // 1. Allocazione della memoria per la nuova entry
    entry_hash_table* new_entry = (entry_hash_table*)malloc(sizeof(entry_hash_table));
    if (new_entry == NULL) {
        return -1; // Errore: memoria esaurita
    }
    
    // 2. Inizializzazione dei campi del nuovo nodo
    new_entry->token = strdup(token); // Alloca e copia la stringa
    if (new_entry->token == NULL) {
        free(new_entry);
        return -1; // Errore nell'allocazione della stringa
    }
    new_entry->testa = NULL;       // La lista dei valori interni parte come vuota

    // 3. Inserimento in testa alla lista concatenata (O(1))
    new_entry->next_entry = ht->arr_entry[index]; // aggancia la catena esistente
    ht->arr_entry[index] = new_entry;             // diventa la nuova testa

    return 0; // Inserimento completato con successo
}

// val è il processed_token restituito da mapper su pipe B
int hashtable_insert(hash_table* ht, char* token, void* val, size_t dim_val){ 
    // controllo degli input
    if(!ht || !token || !val){
        return -1;
    }

    // Inserimento in coda del token in una lista concatenata
    size_t indice_entry = hash(token, ht->dim_tabella);
    entry_hash_table* punt = ht->arr_entry[indice_entry]; // puntatore alla lista di entry corrente

    while((punt != NULL) && (strcmp(punt->token, token)) != 0){ // scrorro la lista di entry
        punt=punt->next_entry;
    }

    if(punt != NULL){ // abbiamo trovato il token
        if(inserisci_valore_opaco_nel_nodo(punt, val, dim_val) != 0){
            fprintf(stderr, "inserimento in testa del valore opaco fallito");
            return -1;
        }
        return 0; // inserisco il valore opaco sull'entry esistente, e basta
    } else { // non abbiamo trovato il token
    // Se ne trovo il token già in lista, creo una nuova entry e la inserisco in testa
        if(hash_table_insert_head(ht, indice_entry, token) != 0){
            fprintf(stderr, "inserimento in testa del token fallito");
            return -1;
        }

        // Inserimento in coda del valore opaco corrispondente al token
        if(inserisci_valore_opaco_nel_nodo(ht->arr_entry[indice_entry], val, dim_val) != 0){
            fprintf(stderr, "inserimento in testa del valore opaco fallito");
            return -1;
        }
    }
    
    return 0;
}


int hashtable_destroy(hash_table* ht){ 
    // prima dealloco tutti i nodi
    for(size_t i=0; i<ht->dim_tabella; i++){ // iteriamo sulla tabella hash
        entry_hash_table* punt = ht->arr_entry[i];
        while(punt != NULL){ // iteriamo sulle sue entry
            nodo* punt_nodo = punt->testa;
            while (punt_nodo != NULL){
                punt->testa = punt->testa->next;
                free(punt_nodo->punt_byte);
                free(punt_nodo);
                punt_nodo = punt->testa;
            }
            ht->arr_entry[i] = ht->arr_entry[i]->next_entry; // per non perdere il puntatore di testa
            free(punt->token);
            free(punt); // libero la entry_hash_table
            punt = ht->arr_entry[i]; // procedo
        }
    }
    free(ht->arr_entry); // libero l'array dei bucket
    
    return 0;
}