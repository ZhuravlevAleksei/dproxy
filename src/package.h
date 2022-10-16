#ifndef PACKAGE_H
#define PACKAGE_H

#include <netinet/in.h>

void datagram_to_pack(struct sockaddr_in *addr, char *buf, int buf_len);
void datagram_to_json();

#endif /* #ifndef PACKAGE_H */
