#ifndef MR_INTERNAL_H
#define MR_INTERNAL_H
#include "../include/mr.h"

struct mr {
    size_t mapper_threads;
    size_t reducer_threads;
    size_t queue_size;
    const char *log_file;
    mr_hash_t hash;
    void *hash_arg;
    mr_mapper_t mapper;
    mr_reducer_t reducer;
    void *user_arg;
};

#endif