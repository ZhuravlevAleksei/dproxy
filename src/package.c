#include "package.h"
#include "logger.h"
#include <unistd.h>
#include <resolv.h>
#include <string.h>
#include <stdlib.h>

#define NS_ANSWER_TTL 60
#define NS_ANSWER_NAME_LOCATION 0xC00C
#define NS_ANSWER_LENGTH 16

static const char *query_json_keys[] = {
    "flags",
    "questions",
    "answer",
    "authority",
    "additional"};

typedef enum
{
    ns_transaction = 0,
    ns_flags = 2,
    ns_questions = 4,
    ns_answer = 6,
    ns_authority = 8,
    ns_additional = 10
} NsSectDisp;

static struct timespec *lock_time;

bool str_to_register(unsigned short *reg, const cJSON *json, const char *key);

void init_package(struct timespec *l_time)
{
    lock_time = l_time;;
}

void create_package(PkgContext *context)
{
    context->datagram = cJSON_CreateObject();
}

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
        error_log(lock_time, "cJSON_CreateNumber == NULL\n");
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
        error_log(lock_time, "cJSON_CreateArray == NULL\n");
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
        error_log(lock_time, "cJSON_CreateString == NULL\n");
        return;
    }

    cJSON_AddItemToObject(j_obj, key, str);
}

unsigned short _msg_parse(const unsigned char *p_msg, NsSectDisp disp)
{
    return ntohs(*((u_int16_t *)(p_msg + disp)));
}

unsigned short questions_to_json(cJSON *j_arr, unsigned short q_number, ns_rr *rr)
{
    unsigned short byte_counter = 0;
    cJSON *j_question = cJSON_CreateObject();

    if (j_question == NULL)
    {
        error_log(lock_time, "questions_to_json == NULL\n");
        return byte_counter;
    }

    cJSON_AddItemToArray(j_arr, j_question);

    create_string_json(j_question, "name", ns_rr_name(*rr));
    byte_counter += strlen(ns_rr_name(*rr)) + 2;

    arithmetic_to_json(j_question, "type", ns_rr_type(*rr));
    arithmetic_to_json(j_question, "class", ns_rr_class(*rr));
    byte_counter += 4;
    return byte_counter;
}

void datagram_to_json(PkgContext *context, struct sockaddr_in *addr, char *buf, int buf_len)
{
    ns_msg msg;
    ns_rr rr;
    int result;
    unsigned short msg_count;
    unsigned short i;
    cJSON *j_queries;
    unsigned short byte_counter = 0;
    char *add_section;
    unsigned char *msg_ptr;

    if (context->datagram == NULL)
    {
        error_log(lock_time, "cJSON_CreateObject == NULL\n");
        return;
    }

    arithmetic_to_json(context->datagram, "addr", addr->sin_addr.s_addr);
    arithmetic_to_json(context->datagram, "port", ntohs(addr->sin_port));

    result = ns_initparse(buf, buf_len, &msg);

    if (result == -1)
    {
        error_log(lock_time, "ns_initparse\n");
        return;
    }

    msg_count = ns_msg_count(msg, ns_s_qd);

    arithmetic_to_json(context->datagram, "transaction", msg._id);
    arithmetic_to_json(context->datagram, "flags", msg._flags);
    arithmetic_to_json(context->datagram, "questions", msg_count);
    arithmetic_to_json(context->datagram, "answer", _msg_parse(msg._msg, ns_answer));
    arithmetic_to_json(context->datagram, "authority", _msg_parse(msg._msg, ns_authority));
    arithmetic_to_json(context->datagram, "additional", _msg_parse(msg._msg, ns_additional));
    byte_counter += 12;

    j_queries = create_queries_json(context);

    for (i = 0; i < msg_count; i++)
    {
        result = ns_parserr(&msg, ns_s_qd, i, &rr);

        if (result < 0)
        {
            error_log(lock_time, "ns_parserr\n");
            return;
        }

        byte_counter += questions_to_json(j_queries, i, &rr);

        result = ns_skiprr(msg._msg_ptr, msg._eom, ns_s_qd, 1);

        if (result < 0)
        {
            break;
        }

        msg._msg_ptr += result;
    }

    if (byte_counter < buf_len)
    {
        msg_ptr = (unsigned char *)(msg._msg + byte_counter);

        byte_counter = buf_len - byte_counter;

        add_section = calloc((byte_counter * 2) + 1, sizeof(char));

        for (i = 0; i < byte_counter; i++)
        {
            sprintf((add_section + (i * 2)), "%02x", *(msg_ptr++));
        }

        if (cJSON_AddStringToObject(context->datagram, "add_section", add_section) == NULL)
        {
            error_log(lock_time, "Additioanal section error\n");
            free(add_section);
            return;
        }

        free(add_section);
    }

    context->json_dump = cJSON_PrintUnformatted(context->datagram);
}

struct DNS_HEADER
{
    unsigned short id; // identification number

    unsigned short flags;

    unsigned short q_count;
    unsigned short ans_count;
    unsigned short auth_count;
    unsigned short add_count;
};

struct DNS_QUESTION
{
    char *qname;
    unsigned short qtype;
    unsigned short qclass;
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
        error_log(lock_time, "json_to_question:cJSON_GetArrayItem error: 'name'\n");
        return 0;
    }

    ChangetoDnsNameFormat(qinfo, j_info->valuestring);

    len += strlen((const char *)qinfo) + 1;
    qinfo += len;

    j_info = cJSON_GetObjectItemCaseSensitive(j_item, "type");

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        error_log(lock_time, "str_to_register error: 'type'\n");
        return 0;
    }
    *((unsigned short *)qinfo) = htons(j_info->valueint);

    qinfo += sizeof(unsigned short);

    j_info = cJSON_GetObjectItemCaseSensitive(j_item, "class");

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        error_log(lock_time, "str_to_register error: 'class'\n");
        return 0;
    }
    *((unsigned short *)qinfo) = htons(j_info->valueint);

    qinfo += sizeof(unsigned short);
    return len += 4;
}

bool str_to_register(unsigned short *reg, const cJSON *json, const char *key)
{
    const cJSON *j_int = cJSON_GetObjectItemCaseSensitive(json, key);

    if (cJSON_IsNumber(j_int) && (j_int != NULL))
    {
        *reg = (unsigned short)j_int->valueint;
        return true;
    }

    return false;
}

bool json_to_datagram(char *packet, unsigned char *resolv, int *res_len, int buf_len)
{
    cJSON *datagram_json = cJSON_Parse(packet);
    const cJSON *j_arr = NULL;
    cJSON *j_string;
    struct DNS_HEADER *dns = NULL;
    struct QUESTION *qinfo = NULL;
    unsigned char *qname;
    unsigned short tmp;
    int i;
    int len = 0;
    char test_str[2];

    if (datagram_json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            error_log(lock_time, "Error before: %s\n", error_ptr);
        }
        return false;
    }

    dns = (struct DNS_HEADER *)resolv;

    dns->id = (unsigned short)htons(getpid());

    unsigned short *reg_ptr = &(dns->flags);

    for (i = 0; i < (sizeof(query_json_keys) / sizeof(const char *)); i++)
    {
        if (!str_to_register(&tmp, datagram_json, query_json_keys[i]))
        {
            error_log(lock_time, "str_to_register error: '%s'\n", query_json_keys[i]);
            *res_len = 0;
            cJSON_Delete(datagram_json);
            return false;
        }
        *(reg_ptr + i) = htons(tmp);
    }

    // pointer of the query
    *res_len = sizeof(struct DNS_HEADER);
    qname = resolv + *res_len;

    j_arr = cJSON_GetObjectItemCaseSensitive(datagram_json, LIST_QUERIES_KEY);

    tmp = json_to_question(qname, j_arr);

    if (tmp == 0)
    {
        *res_len = 0;
        cJSON_Delete(datagram_json);
        return false;
    }

    *res_len += tmp;
    qname = qname + tmp;

    j_string = cJSON_GetObjectItemCaseSensitive(datagram_json, "add_section");

    if (j_string == NULL || j_string->valuestring == NULL)
    {
        *res_len = 0;
        cJSON_Delete(datagram_json);
        return false;
    }

    if (cJSON_IsString(j_string) && (j_string->valuestring != NULL))
    {
        for (i = 0; i < strlen(j_string->valuestring); i += 2)
        {
            test_str[0] = *(j_string->valuestring + i);
            test_str[1] = *(j_string->valuestring + (i + 1));

            if (sscanf(test_str, "%hhx", qname + i / 2) != 1)
            {
                cJSON_Delete(datagram_json);
                *res_len = 0;
                return false;
            }
        }
    }

    *res_len += strlen(j_string->valuestring) / 2;

    cJSON_Delete(datagram_json);
    return true;
}

bool json_to_response(
    unsigned long *out_host, unsigned short *out_port,
    unsigned char *buf, int *buf_len, char *packet)
{
    cJSON *datagram_json = cJSON_Parse(packet);
    cJSON *j_string;
    cJSON *j_int;
    int i;
    char test_str[2];
    unsigned short out_transaction;

    j_int = cJSON_GetObjectItemCaseSensitive(datagram_json, "addr");

    if (!cJSON_IsNumber(j_int) || (j_int == NULL))
    {
        cJSON_Delete(datagram_json);
        return false;
    }

    *out_host = (unsigned long)j_int->valuedouble;

    if (!str_to_register(out_port, datagram_json, "port"))
    {
        cJSON_Delete(datagram_json);
        return false;
    }

    if (!str_to_register(&out_transaction, datagram_json, "transaction"))
    {
        cJSON_Delete(datagram_json);
        return false;
    }

    j_string = cJSON_GetObjectItemCaseSensitive(datagram_json, "datagram");
    if (!cJSON_IsString(j_string) || (j_string->valuestring == NULL))
    {
        cJSON_Delete(datagram_json);
        return false;
    }

    for (i = 0; i < strlen(j_string->valuestring); i += 2)
    {
        test_str[0] = *(j_string->valuestring + i);
        test_str[1] = *(j_string->valuestring + (i + 1));

        if (sscanf(test_str, "%hhx", buf + i / 2) != 1)
        {
            cJSON_Delete(datagram_json);
            return false;
        }
    }

    *buf_len = strlen(j_string->valuestring) / 2;

    *((unsigned short *)buf) = htons(out_transaction);

    cJSON_Delete(datagram_json);
    return true;
}

bool response_to_json(
    char *buf, unsigned long out_host, unsigned short out_port,
    unsigned short out_transaction, char *message)
{
    char *string = NULL;
    cJSON *response_json = cJSON_CreateObject();

    if (cJSON_AddNumberToObject(response_json, "addr", out_host) == NULL)
    {
        cJSON_Delete(response_json);
        return false;
    }

    if (cJSON_AddNumberToObject(response_json, "port", out_port) == NULL)
    {
        cJSON_Delete(response_json);
        return false;
    }

    if (cJSON_AddNumberToObject(response_json, "transaction", out_transaction) == NULL)
    {
        cJSON_Delete(response_json);
        return false;
    }

    if (cJSON_AddStringToObject(response_json, "datagram", message) == NULL)
    {
        cJSON_Delete(response_json);
        return false;
    }

    string = cJSON_PrintUnformatted(response_json);
    strcpy(buf, string);
    cJSON_Delete(response_json);
    return true;
}

bool get_j_long(unsigned long *value, const char *key, cJSON *datagram_json)
{
    cJSON *j_info;

    j_info = cJSON_GetObjectItemCaseSensitive(datagram_json, key);

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        error_log(lock_time, "get_j_long error: '%s'\n", key);
        return false;
    }
    *value = j_info->valuedouble;

    return true;
}

bool get_j_short(unsigned short *value, const char *key, cJSON *datagram_json)
{
    cJSON *j_info;

    j_info = cJSON_GetObjectItemCaseSensitive(datagram_json, key);

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        error_log(lock_time, "get_j_short error: '%s'\n", key);
        return false;
    }
    *value = j_info->valueint;

    return true;
}

bool json_get_addr(
    char *packet, unsigned long *addr, unsigned short *port, unsigned short *transaction)
{
    cJSON *datagram_json = cJSON_Parse(packet);
    cJSON *j_info;

    if (!get_j_long(addr, "addr", datagram_json))
    {
        cJSON_Delete(datagram_json);
        return 0;
    }

    if (!get_j_short(port, "port", datagram_json))
    {
        cJSON_Delete(datagram_json);
        return 0;
    }

    if (!get_j_short(transaction, "transaction", datagram_json))
    {
        cJSON_Delete(datagram_json);
        return 0;
    }

    cJSON_Delete(datagram_json);
    return true;
}

int build_answer_dns(unsigned char *buf, unsigned long addr)
{
    *((unsigned short*)(buf + 0)) = htons(NS_ANSWER_NAME_LOCATION);

    *((unsigned short*)(buf + 2)) = htons(ns_t_a);

    *((unsigned short*)(buf + 4)) = htons(ns_c_in);

    *((unsigned long*)(buf + 6)) = htonl(NS_ANSWER_TTL);

    *((unsigned short*)(buf + 10)) = htons(4);

    *((unsigned long*)(buf + 12)) = addr;

    return NS_ANSWER_LENGTH;
}

void build_resp_header(
    unsigned char *buf, unsigned short transaction, unsigned long answ_addr)
{
    struct DNS_HEADER *h_dns = (struct DNS_HEADER *)buf;

    h_dns->id = (unsigned short)htons(transaction);

    if (answ_addr != 0)
    {
        h_dns->flags = htons(0x8000); // NXDomain (3) & Message is a response
        h_dns->ans_count = htons(0x0001);
    }
    else
    {
        h_dns->flags = htons(0x0003); // NXDomain=3 nameser.h ns_r_nxdomain
        h_dns->ans_count = 0x0000;
    }

    h_dns->q_count = htons(0x0001);
    h_dns->auth_count = 0x0000;
    h_dns->add_count = 0x0000;
}

void bytes_to_hex(char *dst, unsigned char *src, int src_len)
{
    int i;

    for (i = 0; i < src_len; i++)
    {
        sprintf((dst + (i * 2)), "%02x", *(src + i));
    }
    *(dst + (src_len * 2)) = 0;
}

bool build_response_packet(
    char *json_str_buf, unsigned char *datagram_resp_buf, int datagram_len, char *packet)
{
    unsigned short resp_port;
    unsigned long resp_addr;
    unsigned short transaction;
    char *message;

    cJSON *datagram_json = cJSON_Parse(packet);
    if (datagram_json == NULL)
    {
        error_log(lock_time, "build_response_packet:cJSON_Parse packet error\n");
        return false;
    }

    if (!get_j_long(&resp_addr, "addr", datagram_json))
    {
        error_log(lock_time, "build_response_packet:get_j_long 'addr' error\n");
        cJSON_Delete(datagram_json);
        return false;
    }

    if (!get_j_short(&resp_port, "port", datagram_json))
    {
        error_log(lock_time, "build_response_packet:get_j_short 'port' error\n");
        cJSON_Delete(datagram_json);
        return false;
    }

    if (!get_j_short(&transaction, "transaction", datagram_json))
    {
        error_log(lock_time, "build_response_packet:get_j_short 'transaction' error\n");
        cJSON_Delete(datagram_json);
        return false;
    }

    cJSON_Delete(datagram_json);

    message = calloc(((datagram_len * 2) + 1), sizeof(char));
    if (message == NULL)
    {
        error_log(lock_time, "filter response:calloc(%u) error\n", datagram_len);
        free(message);
        return false;
    }
    // There's two symbols per byte ((datagram_len * 2) + 1)
    bytes_to_hex(message, datagram_resp_buf, datagram_len);

    response_to_json(json_str_buf, resp_addr, resp_port, transaction, message);

    free(message);
    return true;
}

int build_response_datagram(
    unsigned char *buf, char *packet, unsigned long answ_addr)
{
    int datagram_len = sizeof(struct DNS_HEADER);
    cJSON *j_queries_arr;
    unsigned char *q_dns;
    unsigned short q_dns_len;
    unsigned char *a_dns;
    unsigned short transaction;

    cJSON *datagram_json = cJSON_Parse(packet);
    if (datagram_json == NULL)
    {
        error_log(lock_time, "build_response_datagram:cJSON_Parse packet error\n");
        return 0;
    }

    if (!get_j_short(&transaction, "transaction", datagram_json))
    {
        error_log(lock_time, "build_response_datagram:get_j_short 'transaction' error\n");
        cJSON_Delete(datagram_json);
        return 0;
    }

    build_resp_header(buf, transaction, answ_addr);

    j_queries_arr = cJSON_GetObjectItemCaseSensitive(datagram_json, LIST_QUERIES_KEY);
    if (j_queries_arr == NULL || (!cJSON_IsArray(j_queries_arr)))
    {
        error_log(lock_time, "build_response_datagram:j_queries_arr packet error\n");
        cJSON_Delete(datagram_json);
        return 0;
    }

    q_dns = buf + sizeof(struct DNS_HEADER);

    q_dns_len = json_to_question(q_dns, j_queries_arr);
    if (q_dns_len == 0)
    {
        error_log(lock_time, "build_response_datagram:question dns length = 0\n");
        cJSON_Delete(datagram_json);
        return 0;
    }

    datagram_len += q_dns_len;

    if(answ_addr != 0)
    {
        a_dns = q_dns + q_dns_len;
        datagram_len += build_answer_dns(a_dns, answ_addr);
    }

    cJSON_Delete(datagram_json);

    return datagram_len;
}
