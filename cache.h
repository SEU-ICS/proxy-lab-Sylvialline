#include "csapp.h"
#include "def.h"

typedef struct {
    char key[MAXLINE];
    char *response;
    size_t response_size;
    char *body;
    size_t body_size;
}entry_t;

typedef struct node{
    entry_t *entry;
    struct node *prev, *next;
}node;

entry_t *make_entry(char *key, char *resp);
void freenode(node *p);
void cache_init();
void make_cache_key(url_info *ip, char *key);
int cache_insert(char *key, char *response);
char *cache_lookup(char *key);