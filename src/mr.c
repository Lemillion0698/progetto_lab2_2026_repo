#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include "../include/mr.h" // path relativo
#include "io.h"
#include "coda.h"
#include "mapper.h"
#include "reducer.h"
#include "mr_internal.h"
#include "log.h"



#define MR_DEFAULT_MAPPER_THREADS 1
#define MR_DEFAULT_REDUCER_THREADS 1
#define MR_DEFAULT_QUEUE_SIZE 100
#define MR_DEFAULT_LOG_FILE "mr.log" // Nome di default del file di log


// entry dell'array dove accumulare i dati del risulato
typedef struct {
    size_t token_len;
    char*  token;
    size_t result_len;
    void*  result;
} risultato_t;

static int cmp_str(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static int collect_files(const char* path, char*** arr, size_t* n){
    if (!path){
        fprintf(stderr, "percorso della directory corrente inesistente\n");
        return -1;
    }
    DIR * d;
    /* Apertura directory corrente */
    if ((d = opendir(path)) == NULL){
        fprintf(stderr, "directory corrente non aperta con successo\n");
        return -1;
    }

    struct dirent* entry;
    while ((errno = 0, entry = readdir(d)) != NULL) {
        // gestito i casi in cui si tratta di "." o ".."
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            continue; // salta, non aprire
        }

        // costruisco il path completo per ogni entry 
        size_t len = strlen(path) + 1 + strlen(entry->d_name) + 1;
        char* full_path = malloc(len);
        snprintf(full_path, len, "%s/%s", path, entry->d_name);

        struct stat info;
        if (stat(full_path,&info)== -1){
            fprintf(stderr,"non si conosce il tipo di fullpath");
            free(full_path);
            closedir(d);
            return -1;
        }
        else {
            if (S_ISREG(info.st_mode)){ 
                *arr = realloc(*arr, (*n + 1) * sizeof(char*));
                (*arr)[*n] = full_path;
                (*n)++;
            }
            else if (S_ISDIR(info.st_mode)){
                collect_files(full_path, arr, n); // scansione ricorsiva di una directory interna
                free(full_path);
            }
        }
    }

    // Chiudo la directory corrente
    if(closedir(d) == -1){
        fprintf(stderr, "directory non chiusa con success\n");
        return -1;
    }
    return 0;
}

static int cmp(const void* a, const void* b) {
    return strcmp(((risultato_t*)a)->token, ((risultato_t*)b)->token);
}

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

int mr_start(mr_t mr, const char *input_path, const char *output_path){
    // Controllo sui parametri
    if(mr == NULL || input_path == NULL || output_path == NULL)
        return -1;

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    char msg[256];

    // Effettuo la scansione ricorsiva
    char** files = NULL;
    size_t nfiles = 0;
    
    struct stat st;
    if(stat(input_path, &st) == -1){
        fprintf(stderr, "non si conosce lo stato del file corrente\n");
        return -1;
    }

    if(S_ISREG(st.st_mode)){
        files = malloc(sizeof(char*));
        files[0] = strdup(input_path);
        nfiles = 1;
    } else if(S_ISDIR(st.st_mode)){
        if(collect_files(input_path, &files, &nfiles) == -1){
            fprintf(stderr, "raccolta dei files della directory corrente fallita\n");
            return -1;
        }
        qsort(files, nfiles, sizeof(char*), cmp_str);
    } 
    else {
        fprintf(stderr, "non è né un file, né una directory\n");
        return -1;
    }

    sem_t* sem = sem_open(MR_LOG_SEM_NAME, O_CREAT | O_EXCL, 0644, 1);
    sem_close(sem); // il padre chiude l'handle, ma il semaforo rimane

    // Creazione delle tre pipe
    int fd_A[2], fd_B[2], fd_C[2]; // descrittori di files delle 3 pipe
    if(pipe(fd_A) == -1 || pipe(fd_B) == -1 || pipe(fd_C) == -1){
        snprintf(msg, sizeof(msg), "pipe() fallita: %s", strerror(errno));
        log_write(mr->log_file, MR_LOG_SEM_NAME, "ERRORE", msg);
        return -1;
    }

    // Chiamata della funzione di scrittura del log
    log_write(mr->log_file, MR_LOG_SEM_NAME, "PIPE", "pipe A, B, C create");

    // CREAZIONE DEL PROCESSO MAPPER

    pid_t pid_Mapper = fork();
    if(pid_Mapper == -1){
        snprintf(msg, sizeof(msg), "fork() mapper fallita: %s", strerror(errno));
        log_write(mr->log_file, MR_LOG_SEM_NAME, "ERRORE", msg);
        return -1;
    }
    
    if(pid_Mapper > 0) {   // ← solo nel padre
        snprintf(msg, sizeof(msg), "processo mapper creato, PID=%ld", (long)pid_Mapper);
        log_write(mr->log_file, MR_LOG_SEM_NAME, "PROCESSO", msg);
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

        mapper_run(mr); // tutta la logica dei threads si trova dentro

        
        close(STDOUT_FILENO); // Chiudo il stdout affinché il Reducer riceva l'EOF
        close(STDIN_FILENO);   // chiude pipe A read end

        exit(EXIT_SUCCESS); // senza, il figlio esce dal blocco if e raggiunge il return 0 finale, tornando al chiamante come se fosse il padre.
    }
    

    // CREAZIONE DEL PROCESSO REDUCER

    pid_t pid_Reducer = fork();
    if(pid_Reducer == -1){
        snprintf(msg, sizeof(msg), "fork() mapper fallita: %s", strerror(errno));
        log_write(mr->log_file, MR_LOG_SEM_NAME, "ERRORE", msg);
        return -1;
    }

    if(pid_Reducer > 0) {
        snprintf(msg, sizeof(msg), "processo reducer creato, PID=%ld", (long)pid_Reducer);
        log_write(mr->log_file, MR_LOG_SEM_NAME, "PROCESSO", msg);
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

        reducer_run(mr); // avvio dei threads

        close(STDIN_FILENO);   // chiude pipe B read end
        close(STDOUT_FILENO);  // chiude pipe C write end → EOF al padre

        exit(EXIT_SUCCESS); // senza, il figlio esce dal blocco if e raggiunge il return 0 finale, tornando al chiamante come se fosse il padre
    }

    // ORA SIAMO NEL PADRE

    close(fd_A[0]); // non legge dalla pipe A
    close(fd_B[1]); // non scrive ...
    close(fd_B[0]); // ... né legge dalla pipe B
    close(fd_C[1]); // non scrive sulla pipe C
    
    // MANDO BYTE GREZZI SULLA PIPE A

    // Apertura del file di input
    char buf[256];
    mr_file_line_t linea;
    unsigned long total_lines = 0;

    for(size_t f = 0; f < nfiles; f++){
        FILE* fp = fopen(files[f], "r");
        if(!fp){ 
            snprintf(msg, sizeof(msg), "fopen(%s) fallita: %s", files[f], strerror(errno));
            log_write(mr->log_file, MR_LOG_SEM_NAME, "ERRORE", msg);
            continue;
        }
        unsigned long pos = 1;
        char* riga;
        while((riga = fgets(buf, sizeof buf, fp))){
            linea.file_name     = files[f];
            linea.file_name_len = strlen(files[f]);
            linea.line_number   = pos;
            linea.line          = buf;
            linea.line_len      = strlen(riga);
            writen(fd_A[1], &linea.file_name_len, sizeof(linea.file_name_len));
            writen(fd_A[1], &linea.line_number,   sizeof(linea.line_number));
            writen(fd_A[1], &linea.line_len,       sizeof(linea.line_len));
            writen(fd_A[1], linea.file_name,       linea.file_name_len);
            writen(fd_A[1], linea.line,            linea.line_len);
            pos++; total_lines++;
        }
        fclose(fp);
    }
    snprintf(msg, sizeof(msg), "righe inviate al mapper: %lu", total_lines);
    log_write(mr->log_file, MR_LOG_SEM_NAME, "RIGHE", msg);

    for(size_t f = 0; f < nfiles; f++) free(files[f]);
    free(files);


    close(fd_A[1]); // per segnalare EOF al Mapper: non mando più byte


    // Apertura in scrittura binaria del file output_path
    int fd_out = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        perror("Errore apertura file di output");
        return -1;
    }
    snprintf(msg, sizeof(msg), "file output aperto: %s", output_path);
    log_write(mr->log_file, MR_LOG_SEM_NAME, "FILE", msg);
     
    risultato_t* arr = NULL; // perché la dimensione dell'array cresce dinamicamente, non sappiamo in anticipo quanti risultati arriveranno dalla pipe C
    size_t n = 0;

    while(1) {
        arr = realloc(arr, (n+1)*sizeof(risultato_t)); // allocazione dinamica di n+1 risultato_t

        ssize_t r0 = readn(fd_C[0], &arr[n].token_len, sizeof(arr[n].token_len));
        if(r0 <= 0) break;                          // EOF: esci

        controllo_io(readn(fd_C[0], &arr[n].result_len, sizeof(arr[n].result_len)));

        arr[n].token = malloc(arr[n].token_len);
        controllo_io(readn(fd_C[0], arr[n].token, arr[n].token_len));

        arr[n].result = malloc(arr[n].result_len);
        controllo_io(readn(fd_C[0], arr[n].result, arr[n].result_len));

        n++;
    }

    // ordinare lessicograficamente i token
    qsort(arr, n, sizeof(risultato_t), cmp);

    snprintf(msg, sizeof(msg), "risultati finali prodotti: %zu", n);
    log_write(mr->log_file, MR_LOG_SEM_NAME, "RISULTATI", msg);

    // scrivere sul file di output
    for(size_t i = 0; i < n; i++) {
        controllo_io(writen(fd_out, &arr[i].token_len, sizeof(arr[i].token_len)));
        controllo_io(writen(fd_out, arr[i].token, arr[i].token_len));
        controllo_io(writen(fd_out, &arr[i].result_len, sizeof(arr[i].result_len)));
        controllo_io(writen(fd_out, arr[i].result, arr[i].result_len));
    }

    // libero tutto ciò che ho allocato
    for(size_t i = 0; i < n; i++) {
        free(arr[i].token);
        free(arr[i].result);
    }
    free(arr);

    // Chiudo il lato di lettura della pipe C e il file output_path
    close(fd_C[0]);
    close(fd_out);

    log_write(mr->log_file, MR_LOG_SEM_NAME, "FILE", "file output chiuso");

    // Il padre attende la fine di entrambi i processi figli per evitare processi "zombie"
    waitpid(pid_Mapper, NULL, 0);
    waitpid(pid_Reducer, NULL, 0);

    sem_unlink(MR_LOG_SEM_NAME);

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed = (t_end.tv_sec - t_start.tv_sec) +
                    (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    FILE* stats = fopen("statistics.txt", "a");
    if(stats){
        fprintf(stats, "--- Esecuzione mr_start ---\n");
        fprintf(stats, "Input:            %s\n", input_path);
        fprintf(stats, "Output:           %s\n", output_path);
        fprintf(stats, "Righe lette:      %lu\n", total_lines);
        fprintf(stats, "Token distinti:   %zu\n", n);
        fprintf(stats, "Risultati emessi: %zu\n", n);
        fprintf(stats, "Tempo (s):        %.6f\n\n", elapsed);
        fclose(stats);
    }

    return 0;
}

int mr_destroy(mr_t mr){
    if(mr == NULL){
        return -1;
    }

    free(mr);
    return 0;
}


