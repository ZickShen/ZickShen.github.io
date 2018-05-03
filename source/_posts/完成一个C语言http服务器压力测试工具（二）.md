---
title: 完成一个C语言http服务器压力测试工具（二）
connments: true
date: 2018-03-18 00:26:15
tags: 
	- C
	- Software Development
categories: Notes
desc: 完成一个C语言http服务器压力测试工具（二）
summary: 复习加深入了解的机会~
---

# 0x0 学习资料

https://stackoverflow.com/

http://man7.org/index.html

《Linux/UNIX系统编程手册》

《UNIX网络编程 卷1：套接字API（第三版）》

# 0x1 已加入多线程豪华套餐

还是先把代码放出来慢慢分析

```c
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <asm/errno.h>
#include <errno.h>
#include <fcntl.h>

#define READ_TIMEOUT_S 3
#define READ_TIMEOUT_US 0
#define WRITE_TIMEOUT_S 3
#define WRITE_TIMEOUT_US 0
#define WAIT_COUNT_MAX 3

typedef int file_descriptor;

char response[1024];
static int g_total = 0;
static int g_max_total = 100000;
static int g_con = 0;
static int g_request_count = 0;
static int g_complete = 0;
static int request_lenth;
pthread_t g_print_thread;


static const int port = 80;
static char *host = "localhost";
struct hostent *server;
struct sockaddr_in serverAddr;
static char *request = "GET http://10.10.10.138/ HTTP/1.1\r\n"
        "Host: 10.10.10.138\r\n"
        "Connection: keep-alive\r\n"
        "Cache-Control: max-age=0\r\n"
        "Upgrade-Insecure-Requests: 1\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/65.0.3325.162 Safari/537.36\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n\r\n";

void *print_condition(void *arg){
    struct timeval cur_time;
    int index = 0;
    fprintf(stdout, "index\tseconds_micro_seconds\tconnection_in_this_second\ttotal_connection\n");
    while(1) {
        sleep(1);
        gettimeofday(&cur_time, NULL);
        fprintf(stdout, "%5d\t%21ld\t%25d\t%16d\n",index, cur_time.tv_sec*1000000 + cur_time.tv_usec, g_con, g_total);
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

void set_block_filedes_timeout(file_descriptor filedes) {
    struct timeval tv_out, tv_in;

    tv_in.tv_sec = READ_TIMEOUT_S;
    tv_in.tv_usec = READ_TIMEOUT_US;
    if (setsockopt(filedes, SOL_SOCKET, SO_RCVTIMEO, &tv_in, sizeof(tv_in)) < 0) {
        perror("set rcv timeout error");
        exit(EXIT_FAILURE);
    }

    tv_out.tv_sec = WRITE_TIMEOUT_S;
    tv_out.tv_usec = WRITE_TIMEOUT_US;
    if (setsockopt(filedes, SOL_SOCKET, SO_SNDTIMEO, &tv_out, sizeof(tv_out)) < 0) {
        perror("set rcv timeout error");
        exit(EXIT_FAILURE);
    }
}

int is_nonblock(file_descriptor fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        perror("get fd flags error");
        exit(EXIT_FAILURE);
    }
    return (flags & O_NONBLOCK) ? 1 : 0;
}

int socket_write(file_descriptor sock){
    int nbytes;
    int wait_count = 0;
    int send_suc = 1;
    while (1) {
        nbytes = send(sock, request, request_lenth, 0);

        if (nbytes < 0) {
            if (is_nonblock(sock)) {
                if (EAGAIN == errno || EWOULDBLOCK == errno || EINTR == errno){
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
            if (is_nonblock(sock)) {
                if (EAGAIN == errno || EWOULDBLOCK == errno || EINTR == errno){
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

void *thread_connetion(void *arg){
    file_descriptor socket;

    while(__sync_fetch_and_add(&g_total, 1) < g_max_total){
        __sync_fetch_and_add(&g_con, 1);
        socket = make_socket();
        connect_server(socket);
        set_block_filedes_timeout(socket);
        __sync_fetch_and_add(&g_request_count, 1);
        if(!socket_write(socket)){
            if(!socket_read(socket)){
                __sync_fetch_and_add(&g_complete, 1);
            }
        }
        close(socket);
    }
}

int init_server(){
    server = gethostbyname(host);
    if(server == NULL){
        return -1;
    }

    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);

    return 0;
}

void usage(char *name){
    printf("Usage: %s [thread_count] [total_connection]\nDue to limitation of number of file descriptor, thread count should not greater than 1024.\n", name);
}

int main(int argc, char **argv) {
    int thread_count;
    int err;
    pthread_t thread_id;
    int index;
    request_lenth = strlen(request);

    if(argc != 3){
        usage(argv[0]);
        exit(EXIT_SUCCESS);
    }
    thread_count = atoi(argv[1]);
    g_max_total = atoi(argv[2]);

    err = init_print_thread();
    if(err < 0){
        perror("Create print thread error");
        exit(EXIT_FAILURE);
    }

    err = init_server();
    if(err < 0){
        perror("No such host");
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

接下来会按照函数调用关系从整体到细节来分析这个程序

## 主函数

首先是加入了可控的参数，一个是线程数，另一个是总连接数，使用方法不对的时候调用usage()输出用法并退出程序。

```c
void usage(char *name){
    printf("Usage: %s [thread_count] [total_connection]\nDue to limitation of number of file descriptor, thread count should not greater than 1024.\n", name);
}
```

为了能直观观察连接情况，所以在执行之前，启动一个打印统计信息的现场，同时这个线程之后还要继续用

``` c
    err = init_print_thread();
    if(err < 0){
        perror("Create print thread error");
        exit(EXIT_FAILURE);
    }
```

如果统计都搞不了，那这个测试也没用了，所以失败直接退出。

函数定义如下：

```c
int init_print_thread(){
    return pthread_create(&g_print_thread, NULL, print_condition, NULL);
}

void *print_condition(void *arg){
    struct timeval cur_time;
    int index = 0;
    fprintf(stdout, "index\tseconds_micro_seconds\tconnection_in_this_second\ttotal_connection\n");
    while(g_total < g_max_total) {
        sleep(1);
        gettimeofday(&cur_time, NULL);
        fprintf(stdout, "%5d\t%21ld\t%25d\t%16d\n",index, cur_time.tv_sec*1000000 + cur_time.tv_usec, g_con, g_total);
        __sync_and_and_fetch(&g_con, 0);
        ++index;
    }
}
```

打印的参数中，g_con是这一秒完成的连接数量（发生请求并接受响应成功计一次连接），g_total为目前的总连接数。

考虑到我们只是想进行压力测试，所以对并不想保留数据，而且发起连接的信息都是一样的，所以也可以看到，之前的int port、char \*host = "localhost"、struct hostent \*server、struct sockaddr_in serverAddr都变成了全局变量，在创建连接前对其进行初始化。

```c
    err = init_server();
    if(err < 0){
        perror("No such host");
    }
```

init_server()的内容就是之前的直接搬了过来

```c
int init_server(){
    server = gethostbyname(host);
    if(server == NULL){
        return -1;
    }

    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);

    return 0;
}
```

然后就创建传入参数个线程

```c
   for (index = 0; index < thread_count; ++index){
        err = pthread_create(&thread_id, NULL, thread_connetion, NULL);
        if(err < 0){
            perror("Create print thread error");
            exit(EXIT_FAILURE);
        }
    }
```

最后用在打印的线程中判断是否结束，达到连接总数后结束

``` c
    pthread_join(g_print_thread, NULL);
```

## 线程，启动！

```c
void *thread_connetion(void *arg){
    file_descriptor socket;

    while(__sync_fetch_and_add(&g_total, 1) < g_max_total){
        __sync_fetch_and_add(&g_con, 1);
        socket = make_socket();
        connect_server(socket);
        set_block_filedes_timeout(socket);
        __sync_fetch_and_add(&g_request_count, 1);
        if(!socket_write(socket)){
            if(!socket_read(socket)){
                __sync_fetch_and_add(&g_complete, 1);
            }
        }
        close(socket);
    }
}
```

对于个线程来说，要做的就是两件事，一个是http连接，另一个是计数。

好消息是http连接用的函数都是线程安全的（根据书上写的，没有看SUSv3），所以前者就是把之前的事情放进函数体就好了。

还有一些改动，比如判断是否阻塞，设置超时时限，这个是为了之后在实现I/O复用的时候复用这些函数。

而后面一个操作需要考虑计数所用的变量肯定是各个线程都有的，所以必须要进行处理。

学了汇编的朋友就知道，即使是一句简单的

```c
i++;
```

也往往不是一个指令完成的，可能是这样的三个指令：

```assembly
mov eax, [esi + 1]
add eax, 1
mov [esi + 1], eax
```

假如一个线程在执行完第一句汇编以后，第二个线程开始执行第一个语句，那么结果就不对了。

目前我知道的有锁、信号两种方法，但是好在Linux提供了一系列原子操作，直接用就好了。所谓原子操作中的原子，意思就和这个词的来历一样，就是指不可分割的一个操作，它要么全部不完成，要么完全生效，没有中间状态。

## 改文件描述符上限

目前这个程序线程与连接是一个1:1的关系，在测试时我发现只要线程数目超过1000多一点，就会无法创建连接。搜索后发现是文件描述符上限的问题。

在bash用命令ulimit -a就能看到文件描述符的上限（在我的Ubuntu14上是1024），如果达到这个上限就会出现无法创建新连接的情况，所以需要改一下。我按照 http://blog.csdn.net/kimsoft/article/details/8024216 把上限改高以后就可以超过1000了。

# 0x2 总结与挖掘

第二篇笔记里，实现了大致以1s为周期的信息统计、多线程请求。这次学习中，我觉得还能深挖的东西有：

- 我在提升文件描述符上限后又遇到另一个问题，就是数量达到大概13000的时候出现了Broken pipe的错误，在提升虚拟机配置后问题有所缓解，但是形成原理目前没有头绪。
- 在提升配置后，虽然没有出现错误，但是线程和连接数无法达到1:1的关系了，比如输入./a.out 50000 1000000后，每秒连接数量大致稳定在2w3左右，但是即使是拿主机而非虚拟机访问服务器也是很流畅的，也就是说瓶颈应该还是在我的程序中，但是瓶颈究竟在哪里？