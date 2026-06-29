#include <stdlib.h>
#include "../include/mr.h" // path relativo
#include <unistd.h>
#include <errno.h>
#include "io.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

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

// dichiarazione anticipata di emit_pair
int emit_pair(const char *token, const void *value, size_t value_size, void *emit_arg);

int mr_start(mr_t mr, const char *input_path, const char *output_path){
    // Controllo sui parametri
    if(mr == NULL || input_path == NULL || output_path == NULL)
        return -1;
    
    // Creazione delle tre pipe
    int fd_A[2], fd_B[2], fd_C[2]; // descrittori di files delle 3 pipe
    if(pipe(fd_A) == -1 || pipe(fd_B) == -1 || pipe(fd_C) == -1){
        return -1;
    }

    // CREAZIONE DEL PROCESSO MAPPER

    pid_t pid_Mapper = fork();
    if(pid_Mapper == -1){
        perror("Errore processo Mapper");
        exit(EXIT_FAILURE);
    }
    if(pid_Mapper == 0){ // Siamo nel processo Mapper
        // Duplicazione dei fd
        if(dup2(fd_A[0], STDIN_FILENO) == -1) { // Ora quello che sarà in ingresso sul processo Mapper, lo sarà anche sulla pipe A    
            perror("duplicazione di fd_A[0] non andata a buon fine");
            exit(EXIT_FAILURE);
        }
        if(dup2(fd_B[1], STDOUT_FILENO) == -1) {// Ora quello che sarà in uscita dal processo Mapper, sarà scritto sulla pipe B 
            perror("duplicazione di fd_B[1] non andata a buon fine");
            exit(EXIT_FAILURE);
        }  
        
        // Chiusura dei descrittori che non servono
        close(fd_A[0]); // già duplicato
        close(fd_B[1]); // già duplicato
        close(fd_A[1]); // non serve al figlio, solo al padre
        close(fd_B[0]); // il Mapper non legge dalla pipe B, ma solo dalla pipe A 
        close(fd_C[0]); // il Mapper non legge ...
        close(fd_C[1]); // ...né scrive sulla pipe C

        // Blocco del thread lettore
        // Lettura e controllo dei 3 header della struct mr_file_line_t
        mr_file_line_t linea;
        ssize_t r1;
        while((r1 = readn(STDIN_FILENO, &(linea.file_name_len), sizeof(linea.file_name_len))) > 0){
            controllo_io(r1);
            ssize_t r2 = readn(STDIN_FILENO, &(linea.line_number), sizeof(linea.line_number));
            controllo_io(r2);
            ssize_t r3 = readn(STDIN_FILENO, &(linea.line_len), sizeof(linea.line_len));
            controllo_io(r3);

            // Allocazione dei buffer
            char* punt_buf_file_name = malloc(linea.file_name_len + 1); 
            char* punt_buf_line = malloc(linea.line_len + 1);

            // Lettura e controllo dei 2 payload
            ssize_t r4 = readn(STDIN_FILENO, punt_buf_file_name, linea.file_name_len);
            controllo_io(r4);
            ssize_t r5 = readn(STDIN_FILENO, punt_buf_line, linea.line_len);
            controllo_io(r5);

            // Assegnazione a linea
            linea.file_name = punt_buf_file_name;
            linea.line = punt_buf_line;

            // invozione della callback mapper
            mr->mapper(&linea, emit_pair, NULL, mr->user_arg);

            free(punt_buf_file_name);
            free(punt_buf_line);
        }

        // Chiudo il stdout affinché il Reducer riceva l'EOF
        close(STDOUT_FILENO);
        
        exit(EXIT_SUCCESS); // senza, il figlio esce dal blocco if e raggiunge il return 0 finale, tornando al chiamante come se fosse il padre.
    }
    

    // CREAZIONE DEL PROCESSO REDUCER

    pid_t pid_Reducer = fork();
    if(pid_Reducer == -1){
        perror("Errore process Reducer");
        exit(EXIT_FAILURE);
    }
    if(pid_Reducer == 0){ // Siamo nel processo Reducer
        // Duplicazione dei fd
        if(dup2(fd_B[0], 0) == -1){// deve leggere dalla pipe B
            perror("duplicazione di fd_B[0] non andata a buon fine");
            exit(EXIT_FAILURE);
        }
        if(dup2(fd_C[1], 1) == -1){ // deve scrivere sulla pipe C
            perror("duplicazione di fd_C[1] non andata a buon fine");
            exit(EXIT_FAILURE);
        }

        // Chiusura dei descrittori che non servono
        close(fd_B[0]); // già duplicato
        close(fd_C[1]); // già duplicato
        close(fd_A[0]); // non serve, Reducer non legge ...
        close(fd_A[1]); // né scrive sulla pipe A
        close(fd_C[0]); // Reducer non legge dalla pipe C
        close(fd_B[1]); // Reducer non scrive sulla pipe B

        exit(EXIT_SUCCESS); // senza, il figlio esce dal blocco if e raggiunge il return 0 finale, tornando al chiamante come se fosse il padre
    }

    // ORA SIAMO NEL PADRE

    close(fd_A[0]); // non legge dalla pipe A
    close(fd_B[1]); // non scrive ...
    close(fd_B[0]); // ... né legge dalla pipe B
    close(fd_C[1]); // non scrive sulla pipe C
    
    // MANDO BYTE GREZZI SULLA PIPE A

    // Apertura del file di input
    FILE* fp = fopen(input_path, "r"); // apre il file di input in lettura
    if (fp == NULL){ // controllo se l'apertura è fallita 
        perror("fopen");
        return -1;
    }
    
    char buf[256]; // buffer per la lettura riga per riga

    mr_file_line_t linea;

    unsigned long posizione_corrente = 1;
    char* riga;
    while((riga = fgets(buf, sizeof buf, fp))){ // deve leggere tutto il file intero
        // popolo i campi di linea
        linea.file_name = input_path;
        linea.file_name_len = strlen(input_path);
        linea.line_number = posizione_corrente;
        linea.line = buf;
        linea.line_len = strlen (riga);
        
        // Serializzo prima i campi che non sono puntatori (header), i metadati che descrivono la linea
        writen(fd_A[1], &linea.file_name_len, sizeof(linea.file_name_len));
        writen(fd_A[1], &linea.line_number, sizeof(linea.line_number));
        writen(fd_A[1], &linea.line_len, sizeof(linea.line_len));

        // Serializzo i 2 payload (i contenuti dei dati veri ed effettivi)
        writen(fd_A[1], linea.file_name, linea.file_name_len); // i byte del percorso del file che scrivo sulla pipe
        writen(fd_A[1], linea.line, linea.line_len); // la riga di byte

        posizione_corrente++; // incremento il numero di riga
    }
    close(fd_A[1]); // per segnalare EOF al Mapper: non mando più byte

    
    // Chiusura del file di input
    if(fclose(fp) == EOF){
        perror("fclose");
    }


    // Il padre attende la fine di entrambi i processi figli per evitare processi "zombie"
    waitpid(pid_Mapper, NULL, 0);
    waitpid(pid_Reducer, NULL, 0);

    return 0;
}

int emit_pair(const char *token, const void *value, size_t value_size, void *emit_arg) {
    (void)emit_arg; // soppressione di warning

    // Controllo sulla validità degli elementi della coppia da emettere
    if(token == NULL || value == NULL){
        return -1;
    }

    // Serializzo i dati da mandare sulla pipe B
    size_t token_len = strlen(token) + 1;
    // Mando prima gli header ...
    ssize_t w1 = writen(STDOUT_FILENO, &token_len, sizeof(token_len));
    controllo_io(w1);
    ssize_t w2 = writen(STDOUT_FILENO, &value_size, sizeof(value_size));
    controllo_io(w2);
    // ...poi i payload
    ssize_t w3 = writen(STDOUT_FILENO, token, token_len);
    controllo_io(w3);
    ssize_t w4 = writen(STDOUT_FILENO, value, value_size);
    controllo_io(w4);

    return 0;
}

int mr_destroy(mr_t mr){
    if(mr == NULL){
        return -1;
    }

    free(mr);
    return 0;
}


