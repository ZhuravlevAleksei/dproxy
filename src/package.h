#ifndef PACKAGE_H
#define PACKAGE_H

#include <cJSON.h>
#include <netinet/in.h>
#include <stdbool.h>


#define LIST_QUERIES_KEY "queries"
#define RESPONSE_PACKET_BUF_SIZE 2048

typedef struct {
    cJSON* datagram;
    char* json_dump;
} PkgContext;

void init_package(struct timespec* l_time);
void create_package(PkgContext* context);
void datagram_to_json(PkgContext* context, struct sockaddr_in* addr, char* buf, int buf_len);
void delete_package(PkgContext* context);
bool json_to_datagram(char* packet, unsigned char* resolv, int* res_len, int buf_len);

bool json_get_addr(char* packet, unsigned long* addr, unsigned short* port, unsigned short* transaction);

bool json_to_response(unsigned long* out_host, unsigned short* out_port, unsigned char* buf, int* buf_len,
                      char* packet);

bool response_to_json(char* buf, unsigned long out_host, unsigned short out_port, unsigned short out_transaction,
                      char* message);

int build_response_datagram(unsigned char* buf, char* packet, unsigned long answ_addr);

bool build_response_packet(char* json_str_buf, unsigned char* datagram_resp_buf, int datagram_len, char* packet);

#endif /* #ifndef PACKAGE_H */
