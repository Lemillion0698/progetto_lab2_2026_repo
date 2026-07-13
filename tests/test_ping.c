#include <stdio.h>
#include <stdlib.h>   // malloc, free
#include <string.h>   // strcmp
#include <sys/stat.h> // mkdir
#include "../include/mr.h"

#define FAIL_TEST -1
#define PASS_TEST 0 

int mapper(const mr_file_line_t* linea, mr_emit_pair_t emit, void* emit_arg, void* user_arg){
    (void)user_arg; // serve a silenziare il warning

    if(!linea){
        fprintf(stderr, "linea di testo non valida\n");
        return -1;
    }
    
    char* copia_linea = strndup(linea->line, linea->line_len); // faccio una copia della linea perché il campo line è costante
    
    size_t uno = 1;
    char* parola = strtok(copia_linea, " \n\t");
    while(parola != NULL) {
        emit(parola, &uno, sizeof(size_t), emit_arg);
        parola = strtok(NULL, " \n\t");
    }

    free(copia_linea);
    return 0;
}


int reducer (const char *token, const mr_value_t *values, size_t values_count, mr_emit_result_t emit, void *emit_arg, void *user_arg){
    (void)user_arg;

    if (!token){
        fprintf(stderr, "token inserito, non valido\n");
        return -1;
    }
    size_t somma = 0;
    for(size_t i=0; i<values_count; i++){
        somma += *(size_t*)values[i].data;
    }
    emit(token, &somma, sizeof(size_t), emit_arg);

    return 0;
}

typedef int (*test_fn)(mr_t); // puntatore a una funzione di test

static int test_file_vuoto(mr_t mr) {
    // 1. crea file vuoto
    FILE* f = fopen("tests/empty.txt", "w");
    fclose(f);

    // 2. esegui
    mr_start(mr, "tests/empty.txt", "tests/out_empty");

    // 3. verifica: il file di output deve essere vuoto
    FILE* out = fopen("tests/out_empty", "rb");
    if(!out){
        fprintf(stderr, "file vuoto non aperto con successo\n");
        return FAIL_TEST;
    }
    size_t token_len;
    size_t letti = fread(&token_len, sizeof(size_t), 1, out);
    fclose(out);

    // 4. pulizia
    remove("tests/empty.txt");
    remove("tests/out_empty");

    if (letti == 0) {
        printf("test_file_vuoto: PASS\n");
        return PASS_TEST;
    }
    printf("test_file_vuoto: FAIL\n");
    return FAIL_TEST;
}

// ritorna il conteggio di `token` nell'output, -1 se non trovato
static int leggi_conteggio(const char* output_path, const char* token){
    if(!output_path || !token){
        fprintf(stderr, "parametri di input non validi\n");
        return FAIL_TEST;
    }
    FILE* out = fopen(output_path, "rb");
    if(!out){
        fprintf(stderr, "file vuoto non aperto con successo\n");
        return FAIL_TEST;
    }

    size_t token_len;
    while(fread(&token_len, sizeof(size_t), 1, out) == 1) {
        char* tok = malloc(token_len);
        fread(tok, 1, token_len, out);

        size_t result_len;
        fread(&result_len, sizeof(size_t), 1, out);
        void* result = malloc(result_len);
        fread(result, 1, result_len, out);

        if(strcmp(tok, token) == 0) {
            long count = (long)(*(size_t*)result);
            free(tok); free(result); fclose(out);
            return count;
        }
        free(tok); free(result);
    }
    fclose(out);
    
    return FAIL_TEST; // Arrivo qui se non trovo il token
}

// verifica se il framework gestice il caso di un file di input con una sola riga
int test_riga_singola(mr_t mr) {
    // 1. Creo un file vuoto
    FILE* f = fopen("tests/riga_singola.txt", "w");

    // 1.1 Scrivo su quel file
    const char* str = "Ciao Mbayanga\n";
    fputs(str, f);

    // 1.2 chiudo il file dopo la scrittura per evitare un incidente
    fclose(f);

    // 2. Eseguo il framework su quel file
    if(mr_start(mr, "tests/riga_singola.txt", "tests/out_empty") == -1){
        fprintf(stderr, "Esecuzione del framwork MR fallita");
        return FAIL_TEST;
    }

    int conta_parola_1 = leggi_conteggio("tests/out_empty", "Ciao");
    int conta_parola_2 = leggi_conteggio("tests/out_empty", "Mbayanga");

    
    // 4. pulizia
    remove("tests/riga_singola.txt");  // non "tests/empty.txt"
    remove("tests/out_empty");

    // 5. verifico il conteggio
    if((conta_parola_1 == 1) && (conta_parola_2 == 1)){
        printf("test_riga_singola: PASS\n");
        return PASS_TEST;
    }

    printf("test_riga_singola: FAIL\n");
    return FAIL_TEST;
}

// verifica se il framework gestice il caso limite di un file di input con una sola riga senza newline "\n"
int test_senza_newline(mr_t mr) {
    // 1. Creo il file di input
    FILE* f = fopen("tests/senza_newline.txt", "w");

    // 1.1 Scrivo su quel file
    const char* str = "Ciao Mbayanga";
    fputs(str, f);

    // 1.2 chiudo il file dopo la scrittura per evitare un incidente
    fclose(f);

    // 2. Eseguo il framework su quel file
    if(mr_start(mr, "tests/senza_newline.txt", "tests/out_empty") == -1){
        fprintf(stderr, "Esecuzione del framwork MR fallita");
        return FAIL_TEST;
    }

    int conta_parola_1 = leggi_conteggio("tests/out_empty", "Ciao");
    int conta_parola_2 = leggi_conteggio("tests/out_empty", "Mbayanga");

    
    // 4. pulizia
    remove("tests/senza_newline.txt");  // non "tests/empty.txt"
    remove("tests/out_empty");

    // 5. verifico il conteggio
    if((conta_parola_1 == 1) && (conta_parola_2 == 1)){
        printf("test_senza_newline: PASS\n");
        return PASS_TEST;
    }

    printf("test_senza_newline: FAIL\n");
    return FAIL_TEST;
}

int test_chiavi_duplicate(mr_t mr) {
    const char* input_dir = "test_dup_input_dir";
    const char* file1_path = "test_dup_input_dir/file1.txt";
    const char* file2_path = "test_dup_input_dir/file2.txt";
    const char* output_file = "test_dup_output.mro";

    // 1. Crea una directory di input ad hoc e due file con la parola "cane" in comune
    // Permessi 0777 per permettere lettura/scrittura/esecuzione
    mkdir(input_dir, 0777); 

    FILE* f1 = fopen(file1_path, "w");
    if (!f1) {
        perror("Errore creazione file1");
        return FAIL_TEST;
    }
    fputs("cane gatto\n", f1); // Prima occorrenza di "cane"
    fclose(f1);

    FILE* f2 = fopen(file2_path, "w");
    if (!f2) {
        perror("Errore creazione file2");
        return FAIL_TEST;
    }
    fputs("topo cane leone\n", f2); // Seconda occorrenza di "cane"
    fclose(f2);

    // 2. Chiama mr_start (il framework elaborerà la directory)
    int mr_res = mr_start(mr, input_dir, output_file);
    if (mr_res == -1) {
        printf("FAIL (mr_start ha restituito errore)\n");
        // Pulizia di emergenza prima di uscire
        remove(file1_path);
        remove(file2_path);
        remove(input_dir); 
        return FAIL_TEST;
    }

    // 3. Usa leggi_conteggio() per leggere l'output binario e verificare i conteggi attesi
    // Ci aspettiamo che la parola "cane" compaia esattamente 2 volte.
    int count = leggi_conteggio(output_file, "cane");

    // 4. Pulisce i file temporanei con remove()
    // Nota: in POSIX bisogna prima rimuovere i file dentro la directory, e poi la directory stessa
    remove(file1_path);
    remove(file2_path);
    remove(input_dir);   // Rimuove la directory di input
    remove(output_file); // Rimuove il file di output prodotto da mr_start

    // 5. Stampa PASS o FAIL e ritorna 0 o -1
    if (count == 2) {
        printf("PASS: test_chiavi_duplicate (La parola 'cane' è stata contata 2 volte correttamente)\n");
        return PASS_TEST;
    } else {
        printf("FAIL: test_chiavi_duplicate (Atteso 2, Ottenuto %d per la parola 'cane')\n", count);
        return FAIL_TEST;
    }
}

int test_ordine_lessicografico(mr_t mr) {
    const char* input_dir = "test_lex_dir";
    const char* file_a = "test_lex_dir/a.txt";
    const char* file_b = "test_lex_dir/b.txt";
    const char* file_c = "test_lex_dir/c.txt";
    const char* output_file = "test_lex_out.mro";

    // 1. Creazione della directory temporanea
    mkdir(input_dir, 0777);

    // 2. Creazione dei tre file, ognuno con una parola univoca
    FILE* fa = fopen(file_a, "w");
    if (!fa) return -1;
    fputs("mela\n", fa);
    fclose(fa);

    FILE* fb = fopen(file_b, "w");
    if (!fb) return -1;
    fputs("banana\n", fb);
    fclose(fb);

    FILE* fc = fopen(file_c, "w");
    if (!fc) return -1;
    fputs("kiwi\n", fc);
    fclose(fc);

    // 3. Esecuzione del framework sulla cartella
    int mr_res = mr_start(mr, input_dir, output_file);
    if (mr_res == -1) {
        printf("FAIL: test_ordine_lessicografico (mr_start ha restituito errore)\n");
        // Pulizia file in caso di errore prematuro
        remove(file_a); remove(file_b); remove(file_c);
        remove(input_dir);
        return FAIL_TEST;
    }

    // 4. Verifica dei conteggi nell'output binario
    // Ci aspettiamo che ogni parola sia stata contata esattamente 1 volta
    int count_mela   = leggi_conteggio(output_file, "mela");
    int count_banana = leggi_conteggio(output_file, "banana");
    int count_kiwi   = leggi_conteggio(output_file, "kiwi");

    // 5. Pulizia del filesystem
    remove(file_a);
    remove(file_b);
    remove(file_c);
    remove(input_dir);   // Elimina la cartella (ora vuota)
    remove(output_file); // Elimina l'output del framework

    // 6. Controllo finale dei risultati e stampa dell'esito
    if (count_mela == 1 && count_banana == 1 && count_kiwi == 1) {
        printf("PASS: test_ordine_lessicografico (Tutti e tre i file sono stati elaborati)\n");
        return PASS_TEST;
    } else {
        printf("FAIL: test_ordine_lessicografico (Conteggi errati. Mela: %d, Banana: %d, Kiwi: %d)\n", 
               count_mela, count_banana, count_kiwi);
        return FAIL_TEST;
    }
}

int main(void) {
    mr_attr_t attr;
    mr_attr_init(&attr);
    mr_t mr;
    mr_create(&mr, &attr, mapper, reducer, NULL);

    test_fn tests[] = {
        test_file_vuoto,
        test_riga_singola,
        test_senza_newline,
        test_chiavi_duplicate,
        test_ordine_lessicografico,
    };
    size_t n = sizeof(tests) / sizeof(tests[0]);

    int falliti = 0;
    for (size_t i = 0; i < n; i++) {
        if (tests[i](mr) != PASS_TEST)
            falliti++;
    }

    mr_destroy(mr);

    printf("\n%zu/%zu test passati\n", n - falliti, n);
    return (falliti == 0) ? 0 : 1;
}