#include <stdio.h>
#include "csapp.h"
#include <stdlib.h>
// 캐시 구조체
typedef struct cache_storage
{
  char path[MAXLINE];
  char *body;
  int size;
  struct cache_storage *next;
  struct cache_storage *prev;
} cache;

void *doit(void *vargp);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void insert_cache(char *path, char *body, int size);
int find_cache(path, fd);
void delete_cache();
void pull_cache(cache *find_root);
void get_filetype(char *filename, char *filetype);




/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
// 캐시구조체 연결리스트를 가리키기 위한 root 포인터
cache *root = NULL;
// 캐시 구조체 연결리스트의 size의 총합
int total_cache_size = 0;

// proxy server main 함수
int main(int argc, char **argv) {
  // root = (cache *)Malloc(sizeof(cache));
  printf("%s", user_agent_hdr);
  int listenfd, *connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  listenfd = Open_listenfd(argv[1]);

   while (1)
  {
      clientlen = sizeof(struct sockaddr_storage);
      connfd = Malloc(sizeof(int));
      *connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
      Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
      Pthread_create(&tid, NULL, doit, connfd);
  }
}

// 한 개의 트랜잭션을 실행하는 doit 함수
void *doit(void *vargp)
{   // rio_client : 클라이언트, rio : tiny 서버, proxyfd : 프록시
  int is_static;
  
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], real_tiny_port[MAXLINE];  
  int proxyfd , con_length = 0, tiny_port;
  char *host, *port, *p, *sbuf, path[MAXLINE], tiny_name[MAXLINE], *tmp;
  rio_t rio, rio_client;
  int fd = *((int *)vargp);
  
  // 스레드 분리
  Pthread_detach(pthread_self());
  Free(vargp);

  // host = "localhost";
  // port = "3000";




  // proxyfd = Open_clientfd(host, port);
  Rio_readinitb(&rio_client, fd);
  Rio_readlineb(&rio_client, buf, MAXLINE);
  // printf("rio_client_buf : %s\n", buf);
  
  //uri 를 파싱 - hostname , path , version
  sscanf(buf, "%s %s %s", method, uri, version);
  tmp = strchr(uri, ':');
  strcpy(tmp, tmp+3);
  strncpy(tiny_name, tmp, 9);
  tiny_name[9] = '\0';
  tmp = strchr(tmp, ':');
  strcpy(tmp, tmp+1);
  tiny_port = atoi(tmp);
  sprintf(real_tiny_port, "%d", tiny_port);
  tmp = strchr(tmp, '/');
  strcpy(path, tmp);
  printf("i'm here\n");
  // 캐시 있는지 체크하는 함수
  if (find_cache(path, fd))   //있으면
  { 
    Close(fd);
    return;
  }
  // 프록시와 메인서버 서버 오픈 및 연결
  proxyfd = Open_clientfd(tiny_name, real_tiny_port);
  Rio_readinitb(&rio, proxyfd);

  // 메인 서버에 파싱한 경로를 주기 위해 buf 에 쓰기
  sprintf(buf, "%s %s HTTP/1.0\r\n", method, path);
  Rio_writen(proxyfd, buf, strlen(buf));

  //buf 에 개행문자가 나올 때까지(전부 출력) 한줄씩 proxyfd 에 적을 while문
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(&rio_client, buf, MAXLINE);    
    // 메인서버에 Proxy-Connection 도 보내줄 예정 - 과제 내용
    // if (strstr(buf, "Proxy-Connection:"))
    // {
    //   tmp = strchr(buf,':');
    //   strcpy(buf, tmp+1);
    //   strcpy(buf, "Proxy-Connection: Close");      
    // }
    Rio_writen(proxyfd, buf, strlen(buf));
  }
  Rio_readlineb(&rio, buf, MAXLINE);
  Rio_writen(fd, buf, strlen(buf));
  // get이나 head가 아닌 다른 메소드를 요청하면, 에러 메세지를 보내고, main 루틴으로 돌아온다 , 그리고 연결은 닫고 다음 연결 요청을 기다린다.
  if (strcasecmp(method, "HEAD") && strcasecmp(method, "GET")) 
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  // 메인 서버에서 response header 받아오기
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(&rio, buf, MAXLINE);
    Rio_writen(fd, buf, strlen(buf));
    printf("buf : %s", buf);
    if (strstr(buf, "Content-length"))
    {
      p = strchr(buf, ':');
      
      con_length = atoi(p+1);
      continue;
    }
  }
  // response body 를 이동할 sbuf 를 content_length만큼 할당
  sbuf = Malloc(con_length);
  Rio_readnb(&rio, sbuf, con_length);
  
  // 캐시 크기가 부족하면, delete_cache()
  // while(con_length + total_cache_size > MAX_CACHE_SIZE)
  // {
  //   delete_cache();
  // }
  insert_cache(path, sbuf, con_length);
  Rio_writen(fd, sbuf, con_length);
  Close(proxyfd);
  Close(fd);
}  
void insert_cache(char *path, char *body, int size)
{
  cache *S1 = (cache *)malloc(sizeof(cache));  
  total_cache_size += size;
  strcpy(S1->path, path);
  S1->size = size;
  S1->body = body;
  if (root == NULL)
  {
    root = S1;
    S1->next = root;
    S1->prev = root;
  }
  else
  {
    root->prev->next = S1;
    S1->prev = root->prev;
    root->prev = S1;
    S1 -> next = root;
    root = S1;
  }
}

int find_cache(char *client_path, int client_fd)
{
  cache *find_root = root;
  char *cache_buf[MAXBUF];
  char *filetype[MAXLINE];

  // sprintf(buf, "HTTP/1.0 200 OK\r\n");
  // sprintf(buf, "%sServer: Tiny Web Server\r\n", cashe_buf);
  // sprintf(buf, "%sConnection: close\r\n", cache_buf);
  // sprintf(buf, "%sContent-length: %d\r\n", cache_buf, find_root->size);
  // sprintf(buf, "%sContent-type: %s\r\n\r\n", cache_buf, filetype);
  if (find_root == NULL)
  {
    return 0;
  }

  if (!strcmp(find_root->path, client_path))
  { get_filetype(find_root->path, filetype);

    sprintf(cache_buf, "HTTP/1.0 200 OK\r\n");
    sprintf(cache_buf, "%sServer: Tiny Web Server\r\n", cache_buf);
    sprintf(cache_buf, "%sConnection: close\r\n", cache_buf);
    sprintf(cache_buf, "%sContent-length: %d\r\n", cache_buf, find_root->size);
    sprintf(cache_buf, "%sContent-type: %s\r\n\r\n", cache_buf, filetype);
    Rio_writen(client_fd, cache_buf, strlen(cache_buf));
    printf("\ncache_buf : %s\n", cache_buf);
    Rio_writen(client_fd,find_root->body, find_root->size);
    printf("\nfind_root-> body : %s\n", find_root->body);
    printf("Am i here? \n");

    return 1;

  }
  while(find_root->next != root)
  {
    find_root = find_root->next;
    if (!strcmp(find_root->path, client_path))
    {
      
      sprintf(cache_buf, "HTTP/1.0 200 OK\r\n");
      sprintf(cache_buf, "%sServer: Tiny Web Server\r\n", cache_buf);
      sprintf(cache_buf, "%sConnection: close\r\n", cache_buf);
      sprintf(cache_buf, "%sContent-length: %d\r\n", cache_buf, find_root->size);
      sprintf(cache_buf, "%sContent-type: text/html\r\n\r\n", cache_buf);
      Rio_writen(client_fd, cache_buf, strlen(cache_buf));

      Rio_writen(client_fd,find_root->body, find_root->size);
      pull_cache(find_root);
      return 1;
    }
  }
  return 0;
}

void pull_cache(cache *find_root)
{
  find_root->prev->next = find_root->next;
  find_root->next->prev = find_root->prev;
  root->prev->next = find_root;
  find_root->prev = root->prev;
  root->prev = find_root;
  find_root->next = root;
}

void delete_cache()
{ // 캐시가 하나
 if (root->next = root)
 {
  free(root->body);
  free(root);
  root = NULL;
 }
 else
 {
  root->prev->prev->next = root;
  root->prev = root->prev->prev;
  free(root->prev->body);
  free(root->prev);
 } 
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];
  // Build the HTTP response body - html 파일
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  // Print the HTTP response =
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
   sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}