#ifndef PACKAGE_H
#define PACKAGE_H

#include <netinet/in.h>
#include <cJSON.h>

#define LIST_QUERIES_KEY "queries"

typedef struct
{
    cJSON *datagram;
    char *json_dump;
}PkgContext;

void init_package(PkgContext *context);
void datagram_to_json(PkgContext *context, struct sockaddr_in *addr, char *buf, int buf_len);
void delete_package(PkgContext *context);

#endif /* #ifndef PACKAGE_H */
