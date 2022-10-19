#include "filter.h"

#include <stdio.h>
#include <string.h>
#include "oniguruma.h"
#include <cJSON.h>
#include "package.h"
#include <stdlib.h>

#define ARRAY_FILTER_KEY "queries"

static cJSON *datagram_json;

void filter_test()
{
    int r;
    unsigned char *start, *range, *end;
    regex_t *reg;
    OnigErrorInfo einfo;
    OnigRegion *region;
    OnigEncoding use_encs[1];

    static UChar *pattern = (UChar *)"a(.*)b|[e-f]+";
    static UChar *str = (UChar *)"zzzzaffffffffb";

    use_encs[0] = ONIG_ENCODING_ASCII;
    onig_initialize(use_encs, sizeof(use_encs) / sizeof(use_encs[0]));

    r = onig_new(&reg, pattern, pattern + strlen((char *)pattern),
                 ONIG_OPTION_DEFAULT, ONIG_ENCODING_ASCII, ONIG_SYNTAX_DEFAULT, &einfo);
    if (r != ONIG_NORMAL)
    {
        char s[ONIG_MAX_ERROR_MESSAGE_LEN];
        onig_error_code_to_str((UChar *)s, r, &einfo);
        fprintf(stderr, "ERROR: %s\n", s);
        return;
    }

    region = onig_region_new();

    end = str + strlen((char *)str);
    start = str;
    range = end;
    r = onig_search(reg, str, end, start, range, region, ONIG_OPTION_NONE);
    if (r >= 0)
    {
        int i;

        fprintf(stderr, "match at %d\n", r);
        for (i = 0; i < region->num_regs; i++)
        {
            fprintf(stderr, "%d: (%d-%d)\n", i, region->beg[i], region->end[i]);
        }
    }
    else if (r == ONIG_MISMATCH)
    {
        fprintf(stderr, "search fail\n");
    }
    else
    { /* error */
        char s[ONIG_MAX_ERROR_MESSAGE_LEN];
        onig_error_code_to_str((UChar *)s, r);
        fprintf(stderr, "ERROR: %s\n", s);
        onig_region_free(region, 1 /* 1:free self, 0:free contents only */);
        onig_free(reg);
        onig_end();
        return;
    }

    onig_region_free(region, 1 /* 1:free self, 0:free contents only */);
    onig_free(reg);
    onig_end();
}

unsigned char get_names_list(char **name_list, char *packet)
{
    unsigned char queries_array_len;
    unsigned char names_len = 0;
    const char *error_ptr;
    const cJSON *queries_arr = NULL;
    const cJSON *query_ns = NULL;
    cJSON *name;

    datagram_json = cJSON_Parse(packet);

    if(datagram_json == NULL)
    {
        error_ptr = cJSON_GetErrorPtr();

        if (error_ptr != NULL)
        {
            printf("Error before: %s\n", error_ptr);
        }

        return names_len;
    }

    queries_arr = cJSON_GetObjectItemCaseSensitive(datagram_json, LIST_QUERIES_KEY);

    queries_array_len = cJSON_GetArraySize(queries_arr);

    *name_list = malloc(queries_array_len * sizeof(char));

    cJSON_ArrayForEach(query_ns, queries_arr)
    {
        name = cJSON_GetObjectItemCaseSensitive(query_ns, "name");

        if (!cJSON_IsString(name))
        {
            continue;;
        }

        *(name_list + names_len) = name->valuestring;

        names_len++;
    }

    return names_len;
}

void clear_filter()
{
    cJSON_Delete(datagram_json);
}
