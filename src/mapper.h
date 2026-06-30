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

typedef struct {
    queue_t *queue;
    int fd;
} reader_arg_t;
