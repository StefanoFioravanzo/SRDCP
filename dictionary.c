/*
Dictionary implementation to store all node-parent relations
in the sink node.
*/

#include "core/net/linkaddr.h"

typedef struct dict_entry_s {
    linkaddr_t key;  // the address of the node
    linkaddr_t value;  // the address of the parent
} dict_entry_s;

typedef struct dict_s {
    int len;
    int cap;
    dict_entry_s *entry;
} dict_s, *dict_t;

int dict_find_index(dict_t dict, const linkaddr_t key) {
    for (int i = 0; i < dict->len; i++) {
        if (linkaddr_cmp(&dict->entry[i]->key, &key) != 0) {
            return i;
        }
    }
    return -1;
}

int dict_find(dict_t dict, linkaddr_t *key, int def) {
    int idx = dict_find_index(dict, key);
    return idx == -1 ? def : dict->entry[idx].value;
}

void dict_add(dict_t dict, linkaddr_t key, linkaddr_t value) {
    /*
    Adds a new entry to the Dictionary
    In case the key already exists, it replaces the value
    */
   int idx = dict_find_index(dict, key);
   linkaddr_t dst_value, dst_key;
   linkaddr_copy(&dst_value, &value);
   linkaddr_copy(&dst_key, &key);
   if (idx != -1) {
       dict->entry[idx].value = dst_value;
       return;
   }
   if (dict->len == dict->cap) {
       dict->cap *= 2;
       dict->entry = realloc(dict->entry, dict->cap * sizeof(dict_entry_s));
   }
   dict->len++;
   dict->entry[dict->len].key = dst_key;
   dict->entry[dict->len].value = dst_value;
