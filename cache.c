#include <assert.h>
#include "cache.h"

struct {
    pthread_mutex_t mutex;
    node head;
    size_t size;
}cache;

void freenode(node *p){
    if(p->entry){
        Free(p->entry->response);
        Free(p->entry);
    }
    Free(p);
}

void cache_init(){
    pthread_mutex_init(&cache.mutex, NULL);
    cache.size = 0;
    cache.head.prev = cache.head.next = &cache.head;
}

void make_cache_key(url_info *ip, char *key) {
    char uri[MAX_PATH], *d = uri;
    // strip fragment (followed by #)
    for(char *c = ip->path; *c && *c != '#';) {
        *d++ = *c++;
    }
    *d = '\0';

    sprintf(key, "%s://%s:%s%s", ip->scheme, ip->host, ip->port, uri);
    printf("cache key: %s\n", key);
}

entry_t *make_entry(char *key, char *resp, size_t resp_len){
    entry_t *e = Malloc(sizeof(entry_t));
    strcpy(e->key, key);

    e->response_size = resp_len;
    e->response = Malloc(resp_len);
    memcpy(e->response, resp, resp_len);
    e->body = strstr(e->response, "\r\n\r\n") + 4; // problematic?
    e->body_size = resp_len - (size_t)(e->body - e->response);
    return e;
}

// withdraw p from the cache ring
static void withdraw(node *p){
    node *l = p->prev, *r = p->next;
    l->next = r;
    r->prev = l;
}

// insert p into the first position of the cache ring
static void insert(node *p) {
    node *h = &cache.head, *q = h->next;
    h->next = p; p->prev = h;
    q->prev = p; p->next = q;
}

// return 0 on success, -1 on error
int cache_insert(char *key, char *resp, size_t resp_len) {
    node *h, *p, *q;
    h = &cache.head;
    p = Malloc(sizeof(node));
    p->entry = make_entry(key, resp, resp_len);
    int size = p->entry->body_size;
    if(size > MAX_OBJECT_SIZE){
        freenode(p);
        return -1;
    }
    
    pthread_mutex_lock(&cache.mutex);
    while(cache.size + size > MAX_CACHE_SIZE) {
        q = h->prev;
        withdraw(q);
        cache.size -= q->entry->body_size;
        freenode(q);
    }
    insert(p);
    cache.size += size;
    printf("Current cache size: %ld\n", cache.size);
    pthread_mutex_unlock(&cache.mutex);
    
    return 0;
}

// if successed, copy the response to resp, return 0
// return -1 if missed
int cache_lookup_copy(char *key, char **outbuf, size_t *outlen) {
    node *p, *h = &cache.head;
    pthread_mutex_lock(&cache.mutex);
    for(p = h->next; p != h; p = p->next) {
        if(strcmp(key, p->entry->key) == 0) break;
    }
    if(p != h){
        // hit
        withdraw(p); insert(p);
        *outlen = p->entry->response_size;
        *outbuf = Malloc(*outlen);
        memcpy(*outbuf, p->entry->response, *outlen);
        pthread_mutex_unlock(&cache.mutex);
        return 0;
    }
    pthread_mutex_unlock(&cache.mutex);
    return -1;
}