#include <stdlib.h>
#include <stdio.h>
#include <errno.h>    // errno, EINTR
#include "io.h"

void controllo_io(ssize_t fun){
    if(fun == 0){
        printf("[processo] pipe chiusa\n");
        exit(EXIT_FAILURE);
    }
    if (fun < 0) { 
        perror("funzione_io header");
        exit(EXIT_FAILURE);
    }
}

void controllo_io_2(ssize_t fun){
    if(fun == 0){
        fprintf(stderr,"Nessun byte letto");
    }
    if(fun < 0){
        fprintf(stderr,"Errore read");
        exit(EXIT_FAILURE);
    }
}

/* =========================================================================
 * readn — legge esattamente n byte dal descrittore fd
 *
 * Restituisce:
 *   n        se ha letto tutti i byte richiesti
 *   < n      se la pipe è stata chiusa prima (EOF parziale)
 *   -1       in caso di errore grave
 * ========================================================================= */
ssize_t readn(int fd, void *buf, size_t n) {
    size_t  nleft = n;
    ssize_t nread;
    char   *ptr = (char *)buf; /* cast a char*: l'aritmetica su void* non è C standard */
 
    while (nleft > 0) {
        nread = read(fd, ptr, nleft);
        if (nread < 0) {
            if (errno == EINTR) continue; /* segnale ricevuto: riprovo */
            return -1;                    /* errore grave di I/O */
        }
        if (nread == 0) break;            /* EOF: lato scrittura chiuso */
 
        nleft -= (size_t)nread;
        ptr   += nread;
    }
    return (ssize_t)(n - nleft);
}
 
 
/* =========================================================================
 * writen — scrive esattamente n byte sul descrittore fd
 *
 * Restituisce:
 *   n    se ha scritto tutti i byte
 *   -1   in caso di errore grave
 * ========================================================================= */
ssize_t writen(int fd, const void *buf, size_t n) {
    size_t       nleft = n;
    ssize_t      nwritten;
    const char  *ptr = (const char *)buf;
 
    while (nleft > 0) {
        nwritten = write(fd, ptr, nleft);
        if (nwritten < 0) {               /* write() segnala errore con -1, MAI con 0 */
            if (errno == EINTR) continue; /* segnale ricevuto: riprovo */
            return -1;
        }
        nleft -= (size_t)nwritten;
        ptr   += nwritten;
    }
    return (ssize_t)n;
}
 

