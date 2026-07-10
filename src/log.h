#ifndef LOG_H
#define LOG_H

#include <semaphore.h>
#include <stdio.h>
#include "../include/mr.h"


#define MR_LOG_SEM_NAME "/mr_log_sem"



int log_write(const char* log_path, const char* sem_name, const char* evento, const char* messaggio);

#endif