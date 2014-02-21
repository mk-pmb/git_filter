#include <string.h>

#include "git2.h"

#include "git_filter.h"
#include "dict.h"

#define GIT_FILTER_LIST_CHUNK_SIZE 8

typedef struct _dict_elem_t
{
    git_oid key;
    const void *value;
} dict_elem_t;

typedef struct _dict_ent_t 
{
    dict_elem_t *dict;
    unsigned int len;
    unsigned int alloc;
} dict_ent_t;

/* two byte table key */
#define TABLE_SIZE (1 << 16)
struct _dict_t 
{
    dict_ent_t table[TABLE_SIZE];
};

dict_t *dict_init()
{
    dict_t *dict = calloc(1, sizeof(dict_t));

    return dict;
}

static inline int _cmp(const git_oid *a, const git_oid *b)
{
    return memcmp(&a->id[2], &b->id[2], GIT_OID_RAWSZ - 2);
}

static int get_pos(dict_ent_t *dict, const git_oid *key)
{
    unsigned int start;
    unsigned int end;

    start = 0;
    end = dict->len;

    for(;;)
    {
        unsigned int middle;
        int cmp;

        if (start == end)
            break;

        middle = (start + end)/2;

        cmp = _cmp(&dict->dict[middle].key, key);
        if (cmp == 0)
            return middle;
        if (cmp < 0)
            start = middle + 1;
        else
            end = middle;
    }

    return -(int)start-1;
}

void dict_add(dict_t *d, const git_oid *key, const void *value)
{
    int pos;
    unsigned int table_key = ((key->id[0] << 8) + key->id[1]);
    dict_ent_t *dict = &d->table[table_key];

    if (dict->len == dict->alloc)
    {
        dict->dict = realloc(dict->dict, sizeof(dict_elem_t)*
                    (GIT_FILTER_LIST_CHUNK_SIZE + dict->alloc));
        A(dict->dict == 0, "realloc failed: no memory");
        dict->alloc += GIT_FILTER_LIST_CHUNK_SIZE;
    }

    pos = get_pos(dict, key);
    A(pos >= 0, "key exists\n");
    pos = -pos-1;

    if (dict->len > pos)
    {
        unsigned int remainder = dict->len - pos;
        memmove(&dict->dict[pos + 1], &dict->dict[pos],
                remainder * sizeof(dict_elem_t));

    }

    dict->dict[pos].key = *key;
    dict->dict[pos].value = value;
    dict->len ++;
}

const void *dict_lookup(dict_t *d, const git_oid *key)
{
    unsigned int table_key = ((key->id[0] << 8) + key->id[1]);
    dict_ent_t *dict = &d->table[table_key];

    int pos = get_pos(dict, key);
    if (pos < 0)
        return 0;

    return dict->dict[pos].value;
}

void dict_dump(dict_t *d, dict_dump_t *dump, void *data)
{
    unsigned int i, t;

    for(t=0; t<TABLE_SIZE; t++)
    {
        dict_ent_t *dict = &d->table[t];

        for(i=0; i<dict->len;i++)
            dump(data, &dict->dict[i].key, dict->dict[i].value);
    }
}
