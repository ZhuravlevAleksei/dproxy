#ifndef STORAGE_H
#define STORAGE_H

#include <hiredis.h>

struct storage
{
    const char *host;
    unsigned short port;
};

void init_srorage(struct storage *conf);
// void write(unsigned short value);
void write_json(char *value);
// void read();


#endif /* #ifndef STORAGE_H */
