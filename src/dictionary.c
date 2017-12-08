/*
Dictionary implementation to store all node-parent relations
in the sink node.
*/

#include "core/net/linkaddr.h"

typedef struct DictEntry {
    linkaddr_t key;  // the address of the node
    linkaddr_t value;  // the address of the parent
} DictEntry;

typedef struct TreeDict {
    int len;
    int cap;
    DictEntry entries[MAX_NODES];
} TreeDict;

int dict_find_index(TreeDict* dict, const linkaddr_t key) {
    for (int i = 0; i < dict->len; i++) {
        if (linkaddr_cmp(&(dict->entries[i]->key), &key) != 0) {
            return i;
        }
    }
    return -1;
}

linkaddr_t dict_find(TreeDict* dict, linkaddr_t *key, int def) {
    int idx = dict_find_index(dict, key);
    return idx == -1 ? def : dict->entries[idx].value;
}

int dict_add(TreeDict* dict, linkaddr_t key, linkaddr_t value) {
    /*
    Adds a new entry to the Dictionary
    In case the key already exists, it replaces the value
    */
   int idx = dict_find_index(dict, key);
   linkaddr_t dst_value, dst_key;
   linkaddr_copy(&dst_value, &value);
   linkaddr_copy(&dst_key, &key);
   if (idx != -1) {  // Element already present, update its value
       dict->entries[idx].value = dst_value;
       return;
   }

   // first initialization
   // if (dict->len == 0) {
   //     dict->len = 1;
   //     dict->cap = 10;
   //     // alloc array of one entry
   //     // dict->
   // }

   if (dict->len == dict->cap) {
       printf("Dict len > Dict cap");
       return -1;
       // dict->cap *= 2;
       // // safe reallocation with tmp variable
       // DictEntry* tmp = realloc(dict->entries, dict->cap * sizeof(DictEntry));
       // if (!tmp) {
       //     // could not resize, handle exception
       //     printf("Could not resize entries in dictionary. ")
       //     return -1;
       // } else {
       //     dict->entries = tmp;
       // }
   }
   dict->len++;
   dict->entries[dict->len].key = dst_key;
   dict->entries[dict->len].value = dst_value;
   return 0;
