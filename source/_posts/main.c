#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <asm/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#define WAIT_COUNT_MAX 3
#define SOCK_LIST_COUNT 40960

#define EPOLLINEV (EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLEXCLUSIVE)

typedef int file_descriptor;

char response[1024];
static int g_total = 0;
static int g_max_total = 0;
static int g_con = 0;
static int g_request_count = 0;
static int g_complete = 0;
static int g_max_con = 0;
static int request_lenth;
static int thread_count;
pthread_t g_print_thread;
file_descriptor epfd;

static const int port = 80;
struct hostent *server;
struct sockaddr_in serverAddr;
static char request[1024];

void *print_condition(void *arg){
    struct timeval cur_time;
    int index = 0;
    fprintf(stdout, "index\tseconds_micro_seconds\tconnection_in_this_second\ttotal_connection\ttotla_requests\n");
    while(g_total < g_max_total) {
        sleep(1);
        gettimeofday(&cur_time, NULL);
        fprintf(stdout, "%5d\t%21ld\t%25d\t%16d\t%14d\n",index, cur_time.tv_sec*1000000 + cur_time.tv_usec, g_con, g_total, g_request_count);
        __sync_and_and_fetch(&g_con, 0);
        ++index;
    }
}

int init_print_thread(){
    return pthread_create(&g_print_thread, NULL, print_condition, NULL);
}

int make_socket(){
    file_descriptor sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("Create socket error");
        exit(EXIT_FAILURE);
    }

    return sock;
}

void connect_server(file_descriptor sock){
    if(connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0){
        perror("Connect server error");
        exit(EXIT_FAILURE);
    }
}

int is_nonblock(int fd, int fflag) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        char *s = fflag ?"read":"write";
        perror(s);
        exit(EXIT_FAILURE);
    }
    return (flags & O_NONBLOCK) ? 1 : 0;
}

void set_nonblock(file_descriptor fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        perror("get fd flags error");
        exit(EXIT_FAILURE);
    }
    flags |= O_NONBLOCK;
    flags = fcntl(fd, F_SETFL, flags);
    if (flags == -1) {
        perror("set fd flags error");
        exit(EXIT_FAILURE);
    }
}

int socket_write(file_descriptor sock){
    int nbytes;
    int wait_count = 0;
    int send_suc = 1;
    while (1) {
        nbytes = send(sock, request, request_lenth, 0);

        if (nbytes < 0) {
            if (is_nonblock(sock, 0)) {
                if (EAGAIN == errno || EWOULDBLOCK == errno || EINTR == errno) {
                    if (wait_count < WAIT_COUNT_MAX) {
                        ++wait_count;
                        usleep(wait_count);
                        continue;
                    }
                }
            }
            break;
        } else if (nbytes == 0) {
            break;
        } else if(nbytes == request_lenth){
            send_suc = 0;
            break;
        }
    }

    return send_suc;
}

int socket_read(file_descriptor sock){
    int nbytes;
    int wait_count = 0;
    int recv_suc = 1;
    while (1) {
        nbytes = recv(sock, response, sizeof(response) - 1, 0);
        if (nbytes < 0) {
            if (is_nonblock(sock, 1)) {
                if (EAGAIN == errno || EWOULDBLOCK == errno || EINTR == errno) {
                    if (wait_count < WAIT_COUNT_MAX) {
                        wait_count++;
                        usleep(wait_count);
                        continue;
                    }
                }
            }
            break;
        }
        if (nbytes == 0) {
            break;
        } else if (nbytes > 0) {
            recv_suc = 0;
        }
    }
    return recv_suc;
}

void make_connection(file_descriptor epfd){
    struct epoll_event ev;
    file_descriptor socket;
    socket = make_socket();
    connect_server(socket);
    set_nonblock(socket);

    ev.data.fd = socket;
    socket_write(socket);
    ev.events = EPOLLINEV;
    epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &ev);

    __sync_fetch_and_add(&g_request_count, 1);
}

void *connect_thread(void *args){
    while (__sync_fetch_and_add(&g_total, 1) < g_max_total){
        if (__sync_fetch_and_add(&g_con, 1) < g_max_con){
            make_connection(epfd);
        } else {
            __sync_fetch_and_add(&g_con, -1);
            __sync_fetch_and_add(&g_total, -1);
        }
    }
}

void *thread_connetion(void *arg){
    file_descriptor nfds;
    int index;
    struct epoll_event events[SOCK_LIST_COUNT];

    while (1){
        usleep(50);
        nfds = epoll_wait(epfd, events, sizeof(events), 500);
        for(index = 0; index < nfds; ++index){
            file_descriptor fd = events[index].data.fd;
            if(events[index].events & EPOLLIN){
                if (!socket_read(fd)){
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, events);
                    close(fd);
                }
            }
        }
    }

}

int init_server(const char *host){
    server = gethostbyname(host);
    if(server == NULL){
        return -1;
    }

	sprintf(request,"GET  HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: keep-alive\r\n"
        "Cache-Control: max-age=0\r\n"
        "Upgrade-Insecure-Requests: 1\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/65.0.3325.162 Safari/537.36\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n\r\n",host);
    request_lenth = strlen(request);

    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);

    return 0;
}

void usage(char *name){
    printf("Usage: %s [thread_count] [concurrent_connections] [total_connection] [hostname]\n"
                   "Due to limitation of number of file descriptor,\n"
                   "connection per thread should not be too large.\n", name);
}

int main(int argc, char **argv) {
    int err;
    pthread_t thread_id;
    int index;

    if(argc != 5){
        usage(argv[0]);
        exit(EXIT_SUCCESS);
    }

    thread_count = atoi(argv[1]);
    g_max_con = atoi(argv[2]);
    g_max_total = atoi(argv[3]);

    assert(thread_count > 0);
    assert(g_max_con > 0);
    assert(g_max_total > 0);

    err = init_print_thread();
    if(err < 0){
        perror("Create print thread error");
        exit(EXIT_FAILURE);
    }

    epfd = epoll_create1(0);

    err = init_server(argv[4]);
    if(err < 0){
        perror("No such host");
    }

    for (index = 0; index < thread_count/10; ++index){
        err = pthread_create(&thread_id, NULL, connect_thread, NULL);
        if(err < 0){
            perror("Create print thread error");
            exit(EXIT_FAILURE);
        }
    }


    for (index = 0; index < thread_count; ++index){
        err = pthread_create(&thread_id, NULL, thread_connetion, NULL);
        if(err < 0){
            perror("Create print thread error");
            exit(EXIT_FAILURE);
        }
    }

    pthread_join(g_print_thread, NULL);
    exit(EXIT_SUCCESS);
}
