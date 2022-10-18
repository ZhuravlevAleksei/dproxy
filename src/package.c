#include "package.h"

// #include <arpa/nameser.h>
#include <arpa/inet.h> //inet_addr , inet_ntoa , ntohs etc
#include <netinet/in.h>
#include <unistd.h> //getpid

#include <resolv.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static const char *query_json_keys[] = {
    "flags",
    "questions",
    "answer",
    "authority",
    "additional"
};

typedef enum
{
    ns_transaction = 0,
    ns_flags = 2,
    ns_questions = 4,
    ns_answer = 6,
    ns_authority = 8,
    ns_additional = 10
} NsSectDisp;

bool str_to_register(unsigned short *reg, const cJSON *json, const char *key);

void init_package(PkgContext *context)
{
    context->datagram = cJSON_CreateObject();
}

// void init_datagram(PkgContext *context)
// {
//     context->datagram_json = cJSON_Parse(packet);
// }

void delete_package(PkgContext *context)
{
    cJSON_Delete(context->datagram);
}

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

cJSON *create_queries_json(PkgContext *context)
{
    cJSON *j_arr = NULL;

    j_arr = cJSON_CreateArray();

    if (j_arr == NULL)
    {
        printf("cJSON_CreateArray\n");
        return j_arr;
    }

    cJSON_AddItemToObject(context->datagram, LIST_QUERIES_KEY, j_arr);

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
    return ntohs(*((u_int16_t *)(p_msg + disp)));
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

void datagram_to_json(PkgContext *context, struct sockaddr_in *addr, char *buf, int buf_len)
{
    ns_msg msg;
    ns_rr rr;
    int result;
    unsigned short msg_count;
    unsigned short i;
    cJSON *j_queries;

    if (context->datagram == NULL)
    {
        printf("cJSON_CreateObject error\n");
        return;
    }

    arithmetic_to_json(context->datagram, "addr", ntohl(addr->sin_addr.s_addr));
    arithmetic_to_json(context->datagram, "port", ntohs(addr->sin_port));

    result = ns_initparse(buf, buf_len, &msg);

    if (result == -1)
    {
        printf("ns_initparse");
        return;
    }

    msg_count = ns_msg_count(msg, ns_s_qd);

    arithmetic_to_json(context->datagram, "transaction", msg._id);
    arithmetic_to_json(context->datagram, "flags", msg._flags);
    arithmetic_to_json(context->datagram, "questions", msg_count);
    arithmetic_to_json(context->datagram, "answer", _msg_parse(msg._msg, ns_answer));
    arithmetic_to_json(context->datagram, "authority", _msg_parse(msg._msg, ns_authority));
    arithmetic_to_json(context->datagram, "additional", _msg_parse(msg._msg, ns_additional));

    j_queries = create_queries_json(context);

    for (i = 0; i < msg_count; i++)
    {
        result = ns_parserr(&msg, ns_s_qd, i, &rr);

        if (result < 0)
        {
            printf("ns_parserr error");
            return;
        }

        questions_to_json(j_queries, i, &rr);

        result = ns_skiprr(msg._msg_ptr, msg._eom, ns_s_qd, 1);

        if (result < 0)
        {
            break;
        }

        msg._msg_ptr += result;
    }

    context->json_dump = cJSON_PrintUnformatted(context->datagram);
}

struct DNS_HEADER
{
    unsigned short id; // identification number

    unsigned short flags;

    unsigned short q_count;    // number of question entries
    unsigned short ans_count;  // number of answer entries
    unsigned short auth_count; // number of authority entries
    unsigned short add_count;  // number of resource entries
};

struct QUESTION
{
    unsigned short qtype;
    unsigned short qclass;
};

void ChangetoDnsNameFormat(unsigned char *dns, unsigned char *host)
{
    int lock = 0, i;
    strcat((char *)host, ".");

    for (i = 0; i < strlen((char *)host); i++)
    {
        if (host[i] == '.')
        {
            *dns++ = i - lock;
            for (; lock < i; lock++)
            {
                *dns++ = host[lock];
            }
            lock++;
        }
    }
    *dns++ = '\0';
}

unsigned short json_to_question(unsigned char *qinfo, const cJSON *j_info_array)
{
    unsigned short len = 0;
    unsigned short tmp;
    const cJSON *j_info;
    const cJSON *j_item;

    j_item = cJSON_GetArrayItem(j_info_array, 0);

    j_info = cJSON_GetObjectItemCaseSensitive(j_item, "name");

    if ((j_info == NULL) || (!cJSON_IsString(j_info)))
    {
        fprintf(stderr, "json_to_question:cJSON_GetArrayItem error: 'name'\n");
        return 0;
    }

    ChangetoDnsNameFormat(qinfo, j_info->valuestring);

    len += strlen((const char *)qinfo) + 1;
    qinfo += len;

    j_info = cJSON_GetObjectItemCaseSensitive(j_item, "type");

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        fprintf(stderr, "str_to_register error: 'type'\n");
        return 0;
    }
    *((unsigned short *)qinfo) = htons(j_info->valueint);

    qinfo += sizeof(unsigned short);

    j_info = cJSON_GetObjectItemCaseSensitive(j_item, "class");

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        fprintf(stderr, "str_to_register error: 'class'\n");
        return 0;
    }
    *((unsigned short *)qinfo) = htons(j_info->valueint);

    return len += 4;
}

bool str_to_register(unsigned short *reg, const cJSON *json, const char *key)
{
    const cJSON *j_int = cJSON_GetObjectItemCaseSensitive(json, key);

    if (cJSON_IsNumber(j_int) && (j_int != NULL))
    {
        *reg = j_int->valueint;
        return true;
    }

    return false;
}

void json_to_datagram(char *packet, unsigned char *resolv, int *res_len, int buf_len)
{
    cJSON *datagram_json = cJSON_Parse(packet);
    const cJSON *j_string = NULL;
    const cJSON *j_arr = NULL;

    struct DNS_HEADER *dns = NULL;
    struct QUESTION *qinfo = NULL;
    unsigned char *qname;
    unsigned short tmp;
    int i;
    int len = 0;

    if (datagram_json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return;
    }

    dns = (struct DNS_HEADER *)resolv;

    dns->id = (unsigned short)htons(getpid());

    unsigned short *reg_ptr = &(dns->flags);

    for (i = 0; i < (sizeof(query_json_keys)/sizeof(const char *)); i++)
    {
        if (!str_to_register(&tmp, datagram_json, query_json_keys[i]))
        {
            fprintf(stderr, "str_to_register error: '%s'\n", query_json_keys[i]);
            *res_len = 0;
            return;
        }
        *(reg_ptr + i) = htons(tmp);
    }

    // pointer of the query
    *res_len = sizeof(struct DNS_HEADER);
    qname = resolv + *res_len;

    j_arr = cJSON_GetObjectItemCaseSensitive(datagram_json, LIST_QUERIES_KEY);

    tmp = json_to_question(qname, j_arr);

    if(tmp == 0)
    {
        *res_len = 0;
        return;
    }

    *res_len += tmp;
}
