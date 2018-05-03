---
title: 完成一个C语言http服务器压力测试工具（三）
connments: true
date: 2018-03-19 22:09:47
tags: 
	- C
	- Software Development
categories: Notes
desc: 完成一个C语言http服务器压力测试工具（三）
summary: 复习加深入了解的机会~
---

# 0x0 学习资料

- 《UNIX网络编程 卷1：套接字API（第三版）》
- 《Linux/UNIX系统编程手册》
- [Epoll is fundamentally broken 1/2](https://idea.popcount.org/2017-02-20-epoll-is-fundamentally-broken-12/)
- [Epoll is fundamentally broken 2/2](https://idea.popcount.org/2017-03-20-epoll-is-fundamentally-broken-22/)

# 0x1 学习路途

这次东西深入的比较多，所以先在前面尝试叙述一下学到的一点东西，然后从知识的基础上再来写程序。

## 非阻塞I/O

在上次实现的两个程序中，采用的都是阻塞型I/O模型，所谓阻塞性I/O模型就是指单个线程每次只在一个文件描述符上进行I/O操作（在本例中是一个socket），每次I/O系统调用都会一直阻塞直到完成数据传输，那么只需要出现以下这些情况，那么情况就会变得很麻烦：

- 检查文件描述符上是否需要进行I/O
- 对没有数据到来的socket调用了read()方法
- 需要检查多个文件描述符，查看他们是否可执行I/O

一旦阻塞上，只能等待操作完成才能进行下一步的操作，这对于许多追求效率的场合都是不能忍受的。于是目前的操作系统都引入了非阻塞I/O的方法。pipe、FIFO、socket、设备都支持非阻塞模式，除了open()外，对于socket、pipe这种无法通过open()获取文件描述符的文件来说，可以使用fcntl()的F_SETFL命令启用非阻塞标志。

除此之外，对于我们所要使用的socket来说，可以在socket()函数的第二个参数中或上SOCK_NONBLOCK来使得socket变成非阻塞模式，需要注意的是，非阻塞模式的socket在进行connect()的时候会返回负值，并将errno置为EINPROGRESS。

## I/O多路复用

有了非阻塞I/O模型，那么我们就可以这样提升我们的效率：

首先对目前有的文件描述符做一个检查，查看是否有可以执行I/O的文件描述符，然后对这些文件描述符执行I/O操作（例如这时可以为这个I/O新开一个线程/进程），这样我们的效率就提升上来了。

但是显然不应该对已有的文件描述符形成的集合进行不停的遍历，这样效率会被浪费许多，好在Linux提供了一些I/O多路复用的接口，包括select/poll/epoll(Linux 2.6+)

做一个粗略的比较的话，如果限定在Linux平台上，那么epoll无疑是最好的选择，原因有以下几个：

- 文件描述符范围有限制。select底层用数组实现，最大连接数写死
- 时间复杂度为O(n)。select/poll在获得事件数后，都需要进行遍历来确认。
- 文件描述符拷贝。每次调用select/poll的时候，都会有一次拷贝。

select/poll/epoll作为三个比较成熟的I/O复用模型，还有很多值得说的东西，但介于一个自己能力不够，另一个时间太少，没法在这里展开叙述了。

## 多线程I/O复用

多线程使用epoll的时候，就需要考虑以下问题：

- 各线程的epoll是否是同一个？
  - 如果是的话，如何保证调用epoll_wait()获得的event是独立的
  - 如果不是的话，建立连接的线程该向哪个epoll file descriptor添加
- 各线程的epoll_event是否是同一个？
  - 如果是的话，如何保证他们处理的event是互斥的
  - 如果不是的话，如何保证他们分别的epoll_event交际是空的
- 如何保证各线程处理的socket没有交集？

以及有关性能的其他考虑：

- 考虑这样的情况，如果多个处理连接的线程都闲置了，此时突然来了一个I/O请求，会不会发生多线程的争抢？或者单纯多个线程的唤醒是否就是对性能的一个极大的开销？（答案是是的，这个情况被称为thundering herd）

## 解决途径

在讨论问题前需要说明的是，在epoll的文档中，作者用比较暧昧的口气说epoll的几个内置函数都是线程安全的（个人感觉），但是暂时也没时间把它代码读了，所以只好以此为基础来做后面的实现了。

在读了两篇博文，上网查阅了Linux的资料后，终于解决了基础的问题，答案就是在在Linux2.6.2中新增的EPOLLONESHOT和Linux4.5中新增的EPOLLEXCLUSIVE。

前者想解决的是这样的问题：

一个文件描述符可能被多个epoll_wait()拉出来，那么就会出现一个竞争的问题。将事件的属性或上EPOLLONESHOT就可以让它被一个epoll_wait()拉出来以后在内部被禁用。

后者想解决的则是thundering herd问题：

如果实例被制定了EPOLLEXCLUSIVE，它们发生事件的时候会只向一个epoll file decriptor发出通知，于是就可以让那个对应的线程接受到事件。

了解了上面两个标示后，将给出目前的做法和之后优化的思路。

## 目前思路

{% asset_img flowchart.png flowchart %}

我使用了单个epoll file descritptor，每个线程维护自己的epoll_evets数组的方法。每个socket加入epoll的时候，都设置EPOLLONESHOT和EPOLLEXCLUSIVE解决竞争问题。

我采用了单独开了100线程（后来测试发现，处理读写的线程效率比我想象的还要低，改成了10）用于不断创建socket并完成连接、写数据、将其加入的过程，因为这个实现方式完成读写的效率实在过于低下，只需开这么几个线程建立的连接就可以处理完了，因为这样，这样的实现方式concurrent number只能作为一个上限，测试过程中能有一两秒能达到10000就不错了……

以及连接总数比较高的时候，会出现

{% asset_img snipaste20180320_005219.png problem %}

具体原因不明，还在排查中，多线程单连接没出现过这种问题。

## 优化思路

开profile tools正在跑，准备看看哪些调用耗时较大。

另外从博客中还看到一种新的结构，应该就能实现一个性能不错的压力测试软件了。

就是开一些线程创建socket连接，用单个线程将epoll_event依次加入一个队列里，处理线程分别向队列里取事件并处理，也就是供应商和客户这个多线程模型。

这样做的话有这样的问题：

- 代码复杂度上升。
  - 要实现一个高效的队列，那么可以否决不断malloc/free的做法，而是开辟一块内存固定用来放这个队列，同时应该申请锁或者用信号来避免竞争时间。
  - 因为是有事件才交给线程处理，所以会有一个线程的不断创建与消亡的代价，这个开销也是很大的，目前了解到的高效的做法是用线程池来提升效率。
- 没有解决目前的BUG
  - 还没了解具体原因，如果是基于错误程序做的优化，依然可能产生同样的错误
- socket连接创建加入的操作
  - socket的连接以及加入epoll是由另一些进程处理的，要协同好它们的关系还需要进一步的考虑，这里只能说做了一个比较粗糙的设想，暂时还没有想到/找到更好、更合适的做法。




# 0x2 源代码

``` c
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
```

