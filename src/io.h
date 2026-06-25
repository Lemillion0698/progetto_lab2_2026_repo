#ifndef IO_H
#define IO_H
#include <unistd.h> // read(), write(), ssize_t

void controllo_io(ssize_t );
ssize_t readn(int, void*, size_t);
ssize_t writen(int, const void*, size_t);

#endif