#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define LRU_MAGIC_NUMBER 9999
#define CACHE_BLOCK_COUNT 10
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_fmt = "Host: %s\r\n";
static const char *request_hdr_fmt = "GET %s HTTP/1.0\r\n";
static const char *end_of_hdr = "\r\n";

static const char *conn_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_conn_key = "Proxy-Connection";
static const char *host_key = "Host";
void *thread(void *vargp);

void doit(int connfd);
void parse_uri(const char *uri, char *server_hostname, char *server_path, int *port);
void build_http_header(char *http_header, char *server_hostname, char *server_path, int port, rio_t *client_rio);
int connect_endServer(char *server_hostname, int port);

/*cache function*/
void cache_init();
int cache_find(const char *url);
int cache_eviction();
void cache_LRU(int index);
void cache_obj(const char *uri, char *obj);
void readBefore(int i);
void readAfter(int i);
void writeBefore(int i);
void writeAfter(int i);

typedef struct {
    char obj[MAX_OBJECT_SIZE];
    char url[MAXLINE];
    int LRU;
    int isEmpty;

    int readCnt;                /*count of readers*/
    sem_t cache_block_mutex;    /*protects accesses to cache*/
    sem_t readCnt_mutex;        /*protects accesses to readcnt*/

} cache_block;

typedef struct {
    cache_block cacheobjs[CACHE_BLOCK_COUNT];
    int cache_num;
} Cache;

Cache cache;

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t  clientlen;
    char server_hostname[MAXLINE], port[MAXLINE];
    pthread_t tid;
    struct sockaddr_storage clientaddr;

    cache_init();

    if (argc != 2) {
        fprintf(stderr, "usage :%s <port> \n", argv[0]);
        exit(1);
    }
    Signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);

        Getnameinfo((SA*)&clientaddr, clientlen, server_hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s %s).\n", server_hostname, port);

        Pthread_create(&tid, NULL, thread, (void *) connfd);
    }
    return 0;
}

/**
 * Thread code.
 */
void *thread(void *vargp) {
    int connfd = (int) vargp;
    Pthread_detach(pthread_self());
    doit(connfd);
    Close(connfd);
}

/**
 * Handle a client HTTP transaction.
 */
void doit(int connfd)
{
    int proxy_client_fd, port;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char proxy_request_header [MAXLINE];
    char server_hostname[MAXLINE], server_path[MAXLINE];

    rio_t client_rio, proxy_rio;

    Rio_readinitb(&client_rio, connfd);
    Rio_readlineb(&client_rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    char url_store[100];
    strcpy(url_store, uri);                                         /* store the original url */
    if (strcasecmp(method, "GET")) {
        printf("Proxy does not implement the method");
        return;
    }

    int cache_index;
    if ((cache_index=cache_find(url_store)) != -1) {
         readBefore(cache_index);
         Rio_writen(connfd, cache.cacheobjs[cache_index].obj, strlen(cache.cacheobjs[cache_index].obj));
         readAfter(cache_index);
         return;
    }

    // parse the uri to get server_hostname, server_path and listen port
    parse_uri(uri, server_hostname, server_path, &port);

    // build the proxy request header which will be sent to the end server
    build_http_header(proxy_request_header, server_hostname, server_path, port, &client_rio);

    // proxy connect to the end server
    proxy_client_fd = connect_endServer(server_hostname, port);
    if (proxy_client_fd < 0) {
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&proxy_rio, proxy_client_fd);

    // write the http header to endserver
    Rio_writen(proxy_client_fd, proxy_request_header, strlen(proxy_request_header));

    // receive obj from end server and send it to the client
    char obj[MAX_OBJECT_SIZE];
    int responseSize = 0;
    size_t n;
    while((n=Rio_readlineb(&proxy_rio, buf, MAXLINE)) != 0) {
        responseSize+=n;
        if (responseSize < MAX_OBJECT_SIZE)  {
            strcat(obj, buf);
        }
        Rio_writen(connfd, buf, n);
    }

    Close(proxy_client_fd);

    // cache the obj if its size < MAX
    if (responseSize < MAX_OBJECT_SIZE) {
        cache_obj(url_store, obj);
    }
}

void build_http_header(char *http_header, char *server_hostname, char *server_path, int port, rio_t *client_rio)
{
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

    sprintf(request_hdr, request_hdr_fmt, server_path);

    // get other request header for client rio 
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, end_of_hdr) == 0) {
            break;
        }

        if (!strncasecmp(buf, host_key, strlen(host_key))) {
            strcpy(host_hdr, buf);
            continue;
        }

        // not connection header || proxy connection header || user agent header
        // then copy it to other_hdr unchanged
        if (strncasecmp(buf, conn_key, strlen(conn_key))
                &&strncasecmp(buf, proxy_conn_key, strlen(proxy_conn_key))
                &&strncasecmp(buf, user_agent_key, strlen(user_agent_key))) {
            strcat(other_hdr, buf);
        }
    }

    if (strlen(host_hdr) == 0) {
        sprintf(host_hdr, host_hdr_fmt, server_hostname);
    }
    sprintf(http_header, "%s%s%s%s%s%s%s", 
            request_hdr, 
            host_hdr, 
            conn_hdr, 
            proxy_hdr, 
            user_agent_hdr, 
            other_hdr, 
            end_of_hdr);

    return ;
}

int connect_endServer(char *server_hostname, int port) {
    char portStr[100];
    sprintf(portStr, "%d", port);
    return Open_clientfd(server_hostname, portStr);
}

void parse_uri(const char *uri, char *server_hostname, char *server_path, int *port)
{
    *port = 80;
    char *uri_pos = strstr(uri, "//");

    uri_pos = (uri_pos != NULL? uri_pos + 2 : uri);

    char *port_pos = strstr(uri_pos, ":");
    if (port_pos != NULL) {
        *port_pos = '\0';
        sscanf(uri_pos, "%s", server_hostname);
        sscanf(port_pos+1, "%d%s", port, server_path);
    }
    else {
        port_pos = strstr(uri_pos, "/");
        if (port_pos != NULL) {
            *port_pos = '\0';
            sscanf(uri_pos, "%s", server_hostname);
            *port_pos = '/';
            sscanf(port_pos, "%s", server_path);
        }
        else {
            sscanf(uri_pos, "%s", server_hostname);
        }
    }
    return;
}
/**********************Cache Function**********************/

/**
 * Init the cache.
 */
void cache_init() {
    cache.cache_num = 0;
    int i;
    for(i = 0;i < CACHE_BLOCK_COUNT; i++) {
        cache.cacheobjs[i].LRU = 0;
        cache.cacheobjs[i].isEmpty = 1;
        Sem_init(&cache.cacheobjs[i].cache_block_mutex, 0, 1);
        Sem_init(&cache.cacheobjs[i].readCnt_mutex, 0, 1);
        cache.cacheobjs[i].readCnt = 0;
    }
}

void readBefore(int i) {
    P(&cache.cacheobjs[i].readCnt_mutex);
    cache.cacheobjs[i].readCnt++;
    if (cache.cacheobjs[i].readCnt == 1) {
        P(&cache.cacheobjs[i].cache_block_mutex);
    }
    V(&cache.cacheobjs[i].readCnt_mutex);
}

void readAfter(int i) {
    P(&cache.cacheobjs[i].readCnt_mutex);
    cache.cacheobjs[i].readCnt--;
    if (cache.cacheobjs[i].readCnt == 0) {
        V(&cache.cacheobjs[i].cache_block_mutex);
    }
    V(&cache.cacheobjs[i].readCnt_mutex);
}

void writeBefore(int i) {
    P(&cache.cacheobjs[i].cache_block_mutex);
}

void writeAfter(int i) {
    V(&cache.cacheobjs[i].cache_block_mutex);
}

/**
 * Find whether or not the url is in the cache
 */
int cache_find(const char *url) {
    int i;
    for(i = 0; i < CACHE_BLOCK_COUNT; i++) {
        readBefore(i);
        if ((!cache.cacheobjs[i].isEmpty) && (strcmp(url, cache.cacheobjs[i].url) == 0)) {
            readAfter(i);
            return i;
        }
        readAfter(i);
    }
    return -1;
}

/**
 * Find the empty cacheObj or which cacheObj should be evictioned
 */
int cache_eviction() {
    int min = LRU_MAGIC_NUMBER;
    int minindex = 0;
    int i;
    for(i = 0; i < CACHE_BLOCK_COUNT; i++) {
        readBefore(i);
        if (cache.cacheobjs[i].isEmpty) {           /*choose if cache block empty */
            minindex = i;
            readAfter(i);
            break;
        }
        if (cache.cacheobjs[i].LRU < min) {         /*if not empty choose the min LRU*/
            minindex = i;
            readAfter(i);
            continue;
        }
        readAfter(i);
    }

    return minindex;
}
/**
 * update the LRU number except the new cache one
 */
void cache_LRU(int index) {
    int i;
    for(i=0; i<index; i++)    {
        writeBefore(i);
        if (cache.cacheobjs[i].isEmpty == 0 && i != index) {
            cache.cacheobjs[i].LRU--;
        }
        writeAfter(i);
    }
    i++;
    for(i; i<CACHE_BLOCK_COUNT; i++)    {
        writeBefore(i);
        if (cache.cacheobjs[i].isEmpty == 0 && i != index) {
            cache.cacheobjs[i].LRU--;
        }
        writeAfter(i);
    }
}

/**
 * Cache the uri with associated obj.
 */
void cache_obj(const char *uri, char *obj) {
    int i = cache_eviction();

    writeBefore(i);/*writer P*/

    strcpy(cache.cacheobjs[i].obj, obj);
    strcpy(cache.cacheobjs[i].url, uri);
    cache.cacheobjs[i].isEmpty = 0;
    cache.cacheobjs[i].LRU = LRU_MAGIC_NUMBER;
    cache_LRU(i);

    writeAfter(i);/*writer V*/
}