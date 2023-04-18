#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);
void *thread(void *vargp);
int main(int argc, char **argv) 
{
  int listenfd, *connfd;
  char *host;
  char *port;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, host, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", host, port);
    Pthread_create(&tid, NULL, thread, connfd);
    
  }
}

void *thread(void *vargp) {
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}
void doit(int fd)
{
  int server_fd;
  int is_static, filesize, pot;
  char proxy_buf[MAXLINE], server_buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  char host[MAXLINE], port[MAXLINE], path[MAXLINE];
  char *body, *tmp;
  rio_t rio_1;
  rio_t rio_2;

  Rio_readinitb(&rio_1, fd);
  Rio_readinitb(&rio_2, server_fd);

  // request를 받고 server에 넘기는 코드
  
  Rio_readlineb(&rio_1, proxy_buf, MAXLINE);
  printf("Request headers:\n");   //
  printf("%s\n", proxy_buf);
  sscanf(proxy_buf, "%s %s %s", method, uri, version);
  // printf("method: %s\n", method);
  // printf("uri: %s\n", uri);
  // printf("version: %s\n", version);
  tmp = strchr(uri, ':');
  strcpy(tmp, tmp+3);
  // printf("tmp: %s\n", tmp);
  strncpy(host, tmp, 9);
  // printf("host: %s" , host);
  host[9] = '\0';
  tmp = strchr(tmp, ':');
  strcpy(tmp, tmp + 1);
  pot = atoi(tmp);
  sprintf(port, "%d", pot);
  // printf("port: %s" , port);
  tmp = strchr(tmp, '/');
  strcpy(path, tmp);

  printf("host: %s port: %s path: %s", host, port, path);
  // sprintf(proxy_buf, "%s %s %s\r\n", method, path, "HTTP/1.0");
  server_fd = Open_clientfd(host, port);
  sprintf(proxy_buf, "%s %s HTTP:/1.0\r\n", method, path);
  Rio_readinitb(&rio_2, server_fd);
  Rio_writen(server_fd, proxy_buf, strlen(proxy_buf));
  while((strcmp(proxy_buf, "\r\n"))){
    Rio_readlineb(&rio_1, proxy_buf, MAXLINE);
    printf("%s\n", proxy_buf);
    Rio_writen(server_fd, proxy_buf, strlen(proxy_buf));
  }
  

  //서버에서 response 받아와서 클라이언트한테 넘기는 코드
  Rio_readlineb(&rio_2, server_buf, MAXLINE);

  while((strcmp(server_buf, "\r\n"))){
    printf("response %s\n", server_buf);
    if (strstr(server_buf,"Content-length")){
      filesize = atoi(strchr(server_buf, ':') +1);

    }
    Rio_writen(fd, server_buf, strlen(server_buf));
    Rio_readlineb(&rio_2, server_buf, MAXLINE);
  }
  Rio_writen(fd, server_buf, strlen(server_buf));

  printf("filesize %d\n", filesize);
  body = (char*) Malloc(filesize);
  Rio_readnb(&rio_2, body, filesize);
  printf("body %s ", body);
  Rio_writen(fd, body, filesize);
  free(body);

  



}

