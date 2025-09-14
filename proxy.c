// Hoshi Proxy


#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

void forward(int connfd, char *host, char *port);
void *routine(void *varg); // thread entry
void relay(int connfd);

int parse_url(const char url[], url_info *ip);

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void bad_request(int fd, char cause[]){
    clienterror(fd, cause, "400", "Bad Request", "Hoshi Proxy cannot parse this request");
}
void not_implemented(int fd, char cause[]){
    clienterror(fd, cause, "501", "Not Implemented", "Hoshi Proxy does not implement this method");
}

int main(int argc, char *argv[])
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char host[MAX_HOST], port[MAX_PORT];
    if(argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    cache_init();

    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, host, sizeof(host), port, sizeof(port), 0);
        // printf("Hoshi Proxy: Accepted connection from (%s, %s)\n", host, port);
        forward(connfd, host, port);
        // Close(connfd);
        // printf("Hoshi Proxy: Disconnected with client (%s, %s)\n", host, port);
    }
    return 0;
}

typedef struct {
    int connfd;
    char host[MAX_HOST], port[MAX_PORT];
}arg_t;

void forward(int connfd, char *host, char *port)
{
    pthread_t tid;

    arg_t *arg = Malloc(sizeof(arg_t));
    arg->connfd = connfd;
    strcpy(arg->host, host);
    strcpy(arg->port, port);

    Pthread_create(&tid, NULL, routine, arg);
    Pthread_detach(tid);
}

void *routine(void *varg)
{
    arg_t *arg= (arg_t *)varg;
    pthread_t tid = pthread_self();
    
    printf("Hoshi Proxy(%lu): Accepted connection from (%s, %s)\n", tid, arg->host, arg->port);
    relay(arg->connfd);
    Close(arg->connfd);
    printf("Hoshi Proxy(%lu): Disconnected with client (%s, %s)\n", tid, arg->host, arg->port);
    Free(varg);
    return NULL;
}

void relay(int connfd)
{
    char buf[MAXLINE], request[MAXLINE];
    char method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char name[MAXLINE], data[MAXLINE];
    rio_t rio;
    url_info info;
    pthread_t tid = pthread_self();

    // parse request line
    Rio_readinitb(&rio, connfd);
    if(!Rio_readlineb(&rio, buf, MAXLINE)) return;
    if(sscanf(buf, "%s %s %s", method, url, version) != 3) {
        bad_request(connfd, buf);
        return;
    }
    if(strcasecmp(method, "GET")) {
        not_implemented(connfd, method);
        return;
    }
    if(parse_url(url, &info) < 0) {
        bad_request(connfd, url);
        return;
    }
    sprintf(request, "%s %s %s\r\n", method, info.path, "HTTP/1.0");
    
    // lookup in cache
    printf("Hoshi Proxy(%lu): Cache lookup...\n", tid);
    char key[MAXLINE], *resp;
    make_cache_key(&info, key);
    if((resp = cache_lookup(key)) != NULL) {
        // cache hit
        printf("Hoshi Proxy(%lu): Cached response:\n%s\n", tid, resp);
        Rio_writen(connfd, resp, sizeof(resp));
        return;
    }
    // cache miss
    printf("Hoshi Proxy(%lu): Cache missed!\n", tid);

    // parse request headers
    int host_specified = 0;
    for(;;){
        Rio_readlineb(&rio, buf, MAXLINE);
        if(strcmp(buf, "\r\n"))break;
        if(sscanf(buf, "%s: %s", name, data) != 2) {
            bad_request(connfd, buf);
            return;
        }
        if(strcasecmp(name, "Host") == 0) {
            sprintf(request, "%s%s\r\n", request, buf);
            host_specified = 1;
        }
        else if(strcasecmp(name, "User-Agent")
             && strcasecmp(name, "Connection")
             && strcasecmp(name, "Proxy-Connection"))
        {
            sprintf(request, "%s%s\r\n", request, buf);
        }
    }
    if(!host_specified) {
        sprintf(request, "%sHost: %s\r\n", request, info.host);
    }
    sprintf(request, "%s%s%s%s\r\n", request, user_agent_hdr, connection_hdr, proxy_connection_hdr);
    // request preparation finished
    printf("Hoshi Proxy(%lu): Parsed request:\n%s\n", tid, request);

    // connect with server
    int clientfd, rc;
    int i = 0;
    char response[MAX_OBJECT_SIZE + MAXLINE];
    clientfd = Open_clientfd(info.host, info.port);
    printf("Hoshi Proxy(%lu): Connected with server (%s, %s)\n", tid, info.host, info.port);
    Rio_writen(clientfd, request, strlen(request));

    
    printf("Hoshi Proxy(%lu): Received response:\n", tid);
    while((rc = Rio_readn(clientfd, response, sizeof(response))) != 0) {
        Rio_writen(connfd, response, rc);
        i ++;
        printf("%s", response);
    }
    Close(clientfd);
    printf("\nHoshi Proxy(%lu): Disconnected with server (%s, %s)\n", tid, info.host, info.port);
    printf("\n");
    
    if(i == 1) { 
        // may need to be cached

    }
    
    
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Hoshi Proxy</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */

// do strncpy, then add '\0' at the end
void strncpy0(char* dest, const char* src, size_t count)
{
    strncpy(dest, src, count);
    dest[count] = '\0';
}


// parse url into scheme, host, port and path, which is the
// required uri, in ip structure.
// only work with urls in the form of
// "scheme://host[:port=80]/path[?query][#fragment]"
// "[user:password@]" is not supported
// return 0 on success, -1 on invalid url
int parse_url(const char url[], url_info *ip)
{
    const char *pos1, *pos2, *pos3;
    if(ip == NULL)
        app_error("null ip in parse_url()");

    printf("Hoshi Proxy(%lu):\nParsing \"%s\"...\n", pthread_self(), url);

    if((pos1 = strstr(url, "://")) == NULL){
        printf("invalid url!\n");
        return -1;
    }
    strncpy0(ip->scheme, url, pos1-url);
    pos1 += 3;
    if((pos2 = strchr(pos1, '/')) == NULL) {
        pos2 = url + strlen(url);
        strcpy(ip->path, "/");
    }
    else {
        strcpy(ip->path, pos2);
    }

    if((pos3 = strchr(pos1, ':')) == NULL || pos3 > pos2) {
        pos3 = pos2;
        strcpy(ip->port, "80");
    }
    else{
        pos3 ++;
        strncpy0(ip->port, pos3, pos2-pos3);
        pos3 --;
    }
    strncpy0(ip->host, pos1, pos3-pos1);
    for(char *c = ip->host; *c; c ++) {
        *c = tolower(*c);
    }

    printf("Successfully parsed as:\n"
           "  scheme: %s\n"
           "  host: %s\n"
           "  port: %s\n"
           "  path: %s\n", ip->scheme, ip->host, ip->port, ip->path);
    
    return 0;
}
