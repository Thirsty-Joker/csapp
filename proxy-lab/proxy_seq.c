#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-Connection: close\r\n";
static const char *endOfhdr = "\r\n";

static const char *host_hdr_fmt = "Host: %s\r\n";
static const char *request_hdr_fmt = "GET %s Http/1.0\r\n";

static const char *user_agent_key = "User-Agent";
static const char *conn_key = "Connection";
static const char *proxy_key = "Proxy-Connection";
static const char *host_key = "Host";

void doit(int connfd);
void parse_uri(char *uri, char *client_hostname, char *path, int *port);
void build_proxy_request_hdr(char *endServer_http_header, char *client_hostname, char *path, int port, rio_t *client_rio);
int connect_endServer(char *hostname, int port);

void *thread(void *vargp);

int main(int argc, char const *argv[])
{
	int listenfd, connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;						/* Enough room for any addr */
	char client_hostname[MAXLINE], client_port[MAXLINE];	/* Client hostname and port number */

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	listenfd = Open_listenfd(argv[1]);						/* Proxy listen fd */

	while (1) {
		clientlen = sizeof(struct sockaddr_storage);

		connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);

		Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, \
			client_port, MAXLINE, 0);

		printf("Accept connection from (%s, %s)\n", client_hostname, client_port);

		doit(connfd);

		Close(connfd);
	}
	
	return 0;
}

/**
 * Accept request from client and deliver it to end server 
 */ 
void doit(int connfd)
{
	int proxy_request_fd, server_listen_port;

	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	
	char server_hostname[MAXLINE], server_path[MAXLINE];

	char proxy_request_hdr[MAXLINE];

	rio_t client_rio, proxy_rio;

	Rio_readinitb(&client_rio, connfd);						/* Connect connfd with rio buf */
	Rio_readlineb(&client_rio, buf, MAXLINE);				/* Read hdr from fd, copy it to buf defined on stack */
	sscanf(buf, "%s %s %s", method, uri, version);			/* Read hdr from buf and put it to argument */

	if (strcasecmp(method, "GET")) {
		printf("Proxy does not implement the method\n");
		return;
	}

	// forward request towards end server, parse uri first
	parse_uri(uri, server_hostname, server_path, &server_listen_port);

	build_proxy_request_hdr(proxy_request_hdr, server_hostname, server_path, server_listen_port, &client_rio);

	proxy_request_fd = connect_endServer(server_hostname, server_listen_port);
	
	if (proxy_request_fd < 0) {
		printf("Connection failed\n");
		return;
	}
	printf("%s\n", proxy_request_hdr);

	Rio_readinitb(&proxy_rio, proxy_request_fd);									/* Connect proxy_request_fd with server rio buf */
	Rio_writen(proxy_request_fd, proxy_request_hdr, strlen(proxy_request_hdr));		/* Read hdr from fd, copy it to buf defined on stack */
	
	size_t n;
	while ((n=Rio_readlineb(&proxy_rio, buf, MAXLINE)) != 0) {
		printf("Proxy received %d bytes, then send to server...\n", n);
		Rio_writen(connfd, buf, n);
	}

	Close(proxy_request_fd);
}
/**
 * parse the uri = hostname [+,port] + path
  */
void parse_uri(char *uri, char *server_hostname, char *path, int *port) 
{
	*port = 80;
	char *uriPos = strstr(uri, "//");
	uriPos = (uriPos != NULL ? uriPos+2 : uri);

	
	char *portPos = strstr(uriPos, ":");

	if (portPos != NULL) {
		*portPos = '\0';
		sscanf(uriPos, "%s", server_hostname);
		sscanf(portPos+1, "%d%s", port, path);
	}
	else {
		portPos = strstr(uri, "/");
		if (portPos != NULL) {
			*portPos = '\0';
			sscanf(uriPos, "%s", server_hostname);
			*portPos = '/';
			sscanf(portPos, "%s", path);
		}
		else {
			sscanf(uriPos, "%s", server_hostname);
		}
	}
}

void build_proxy_request_hdr(char *proxy_request_hdr, char *server_hostname, char *path, int port, rio_t *client_rio)
{
	char buf[MAXLINE];
	char request_hdr[MAXLINE], host_hdr[MAXLINE], other_hdr[MAXLINE];

	sprintf(request_hdr, request_hdr_fmt, path);

	while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
		if (strcmp(buf, endOfhdr) == 0) {
			break;
		}

		if (!strncasecmp(buf, host_key, strlen(host_key))) {
			strcpy(host_hdr, buf);
			continue;
		}

		if (strncasecmp(buf, conn_key, strlen(conn_key))
			&& strncasecmp(buf, proxy_key, strlen(proxy_key))
			&& strncasecmp(buf, user_agent_key, strlen(user_agent_key))) {
			strcat(other_hdr, buf);
		}
	}

	if (strlen(host_hdr) == 0) {
		sprintf(host_hdr, host_hdr_fmt, server_hostname);
	}

	sprintf(proxy_request_hdr, "%s%s%s%s%s%s%s", request_hdr, host_hdr, conn_hdr,
		proxy_hdr, user_agent_hdr, other_hdr, endOfhdr);

	return;
}

/**
 * connect end server, return proxy_fd as proxy is the client and end server is the server
 */
int connect_endServer(char *server_hostname, int server_listen_port)
{
	char port[100];
	sprintf(port, "%d", server_listen_port);
	return Open_clientfd(server_hostname, port);
}



