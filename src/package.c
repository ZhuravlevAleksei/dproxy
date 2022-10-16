#include "package.h"

#include <arpa/nameser.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cJSON.h>


static cJSON *datagram;

typedef struct
{
    char *name;
    char *type;
    char *class;
}QueryStr;

typedef struct
{
    char transaction[5];
    char flags[5];
    char questions[5];
    char answer[5];
    char authority[5];
    char additional[5];
    QueryStr *queries;
}DatagramStr;

typedef struct
{
    char *addr;
    char *port;
    DatagramStr datagram;
}Package;

void arithmetic_to_str(char **v_str, unsigned long long value)
{
    char buf[20] = {0};
    size_t len;

    sprintf(buf, "%llx", (unsigned long long)value);

    len = strlen(buf) + 1;

    *v_str = calloc(len, 1);

    if(*v_str == NULL)
    {
        printf("arithmetic_to_str calloc error\n");
    }

    strcpy(*v_str, buf);
}

typedef enum {
	ns_transaction = 0,
	ns_flags = 2,
    ns_questions = 4,
    ns_answer = 6,
    ns_authority = 8,
    ns_additional = 10
} NsSectDisp;

void arithmetic_to_json(cJSON *j_obj, const char *key, unsigned long long value)
{
    cJSON *j_number = NULL;

    j_number = cJSON_CreateNumber(value);

    if (j_number == NULL)
    {
        printf("cJSON_CreateNumber\n");
        return;
    }

    cJSON_AddItemToObject(j_obj, key, j_number);
}

cJSON *create_queries_json()
{
    cJSON *j_arr = NULL;

    j_arr = cJSON_CreateArray();

    if (j_arr == NULL)
    {
        printf("cJSON_CreateArray\n");
        return j_arr;
    }

    cJSON_AddItemToObject(datagram, "queries", j_arr);

    return j_arr;
}

void create_string_json(cJSON *j_obj, const char *key, const char *value)
{
    cJSON *str = NULL;

    str = cJSON_CreateString(value);

    if (str == NULL)
    {
        printf("cJSON_CreateString\n");
        return;
    }

    cJSON_AddItemToObject(j_obj, key, str);
}

unsigned short _msg_parse(const unsigned char *p_msg, NsSectDisp disp)
{
    return ntohs(*((u_int16_t*)(p_msg + disp)));
}

void questions_to_pack(QueryStr *queries, unsigned short q_number, ns_rr *rr)
{
    const char *name;

    name = ns_rr_name(*rr);

    (queries + q_number)->name = calloc((strlen(name) + 1), sizeof(char));

    if((queries + q_number)->name == NULL)
    {
        printf("questions_to_pack calloc error\n");
        return;
    }

    strcpy((queries + q_number)->name, name);


    arithmetic_to_str(&((queries + q_number)->type), ns_rr_type(*rr));
    arithmetic_to_str(&((queries + q_number)->class), ns_rr_class(*rr));
}

void questions_to_json(cJSON *j_arr, unsigned short q_number, ns_rr *rr)
{
    cJSON *j_question = cJSON_CreateObject();

    if (j_question == NULL)
    {
        printf("questions_to_json error\n");
        return;
    }

    cJSON_AddItemToArray(j_arr, j_question);

    create_string_json(j_question, "name", ns_rr_name(*rr));
    arithmetic_to_json(j_question, "type", ns_rr_type(*rr));
    arithmetic_to_json(j_question, "class", ns_rr_class(*rr));
}

void datagram_to_pack(struct sockaddr_in *addr, char *buf, int buf_len)
{
    Package pkg;
    ns_msg msg;
    ns_rr rr;
    int result;
    unsigned short msg_count;
    unsigned short i;

    arithmetic_to_str(&pkg.addr, ntohl(addr->sin_addr.s_addr));
    arithmetic_to_str(&pkg.port, ntohs(addr->sin_port));

    result = ns_initparse(buf, buf_len, &msg);

    if(result == -1)
    {
        printf("ns_initparse");
        return;
    }

    msg_count = ns_msg_count(msg, ns_s_qd);

    sprintf(pkg.datagram.transaction, "%04x", msg._id);
    sprintf(pkg.datagram.flags, "%04x", msg._flags);
    sprintf(pkg.datagram.questions, "%04x", msg_count);
    sprintf(pkg.datagram.answer, "%04x", _msg_parse(msg._msg, ns_answer));
    sprintf(pkg.datagram.authority, "%04x", _msg_parse(msg._msg, ns_authority));
    sprintf(pkg.datagram.additional, "%04x", _msg_parse(msg._msg, ns_additional));

    pkg.datagram.queries = calloc(msg_count, sizeof(QueryStr));

    if(pkg.datagram.queries == NULL)
    {
        printf("pkg.datagram.queries calloc error, msg_count=%u", msg_count);
        return;
    }

    for(i = 0; i < msg_count; i++)
    {
        result =  ns_parserr(&msg, ns_s_qd, i, &rr);

        if(result < 0)
        {
            printf("ns_parserr error");
            return;
        }

        questions_to_pack(pkg.datagram.queries, i, &rr);

        result = ns_skiprr(msg._msg_ptr, msg._eom, ns_s_qd, 1);

        if(result < 0)
        {
            break;
        }

        msg._msg_ptr += result;
    }
}

void datagram_to_json(char **output, struct sockaddr_in *addr, char *buf, int buf_len)
{
    datagram = cJSON_CreateObject();
    ns_msg msg;
    ns_rr rr;
    int result;
    unsigned short msg_count;
    unsigned short i;
    cJSON *j_queries;

    if (datagram == NULL)
    {
        printf("cJSON_CreateObject error\n");
        return;
    }

    arithmetic_to_json(datagram, "addr", ntohl(addr->sin_addr.s_addr));
    arithmetic_to_json(datagram, "port", ntohs(addr->sin_port));

    result = ns_initparse(buf, buf_len, &msg);

    if(result == -1)
    {
        printf("ns_initparse");
        return;
    }

    msg_count = ns_msg_count(msg, ns_s_qd);

    arithmetic_to_json(datagram, "transaction", msg._id);
    arithmetic_to_json(datagram, "flags", msg._flags);
    arithmetic_to_json(datagram, "questions", msg_count);
    arithmetic_to_json(datagram, "answer", _msg_parse(msg._msg, ns_answer));
    arithmetic_to_json(datagram, "authority", _msg_parse(msg._msg, ns_authority));
    arithmetic_to_json(datagram, "additional", _msg_parse(msg._msg, ns_additional));

    j_queries = create_queries_json();

    for(i = 0; i < msg_count; i++)
    {
        result =  ns_parserr(&msg, ns_s_qd, i, &rr);

        if(result < 0)
        {
            printf("ns_parserr error");
            return;
        }

        questions_to_json(j_queries, i, &rr);

        result = ns_skiprr(msg._msg_ptr, msg._eom, ns_s_qd, 1);

        if(result < 0)
        {
            break;
        }

        msg._msg_ptr += result;
    }

    *output = cJSON_PrintUnformatted(datagram);
}

void pack_to_datagram()
{
    
}
