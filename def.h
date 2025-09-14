#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define MAX_SCHEME 16
#define MAX_HOST 256
#define MAX_PORT 16
#define MAX_PATH 1024

typedef struct {
    char scheme[MAX_SCHEME];
    char host[MAX_HOST];
    char port[MAX_PORT];
    char path[MAX_PATH];
}url_info;