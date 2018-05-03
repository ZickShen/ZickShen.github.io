---
title: 完成一个C语言http服务器压力测试工具（一）
connments: true
date: 2018-03-15 22:30:41
tags: 
	- C
	- Software Development
categories: Notes
desc: 完成一个C语言http服务器压力测试工具（一）
summary: 复习加深入了解的机会~
---

一直没有机会拿C/cpp写点有意思的东西，其实内心还是有点戚戚然的，然后这次正好赶上机会了，感觉这个东西代码量也不大，原理值得琢磨，蛮开心的~

# 0x0 计划

个人认为此次工具要攻克的点在于如何用尽可能低的资源来完成压力测试工具。初步的计划是

- 实现http连接
- 加入多线程
- 加入线程内并发连接
- 优化
- 优化
- 无尽的优化

个人理解来一份好的代码总是需要经历重构的，而个人感受来说，过早优化经常会产生第二系统效应，因为精益求精反而连个能用的结果都做不出来

每次都只增加一点东西，但是测试好结果再继续，这样先把东西做出来，做得差不多了再重构

## 学习资料

https://stackoverflow.com/

http://man7.org/index.html

《Linux/UNIX系统编程手册》

《UNIX网络编程 卷1：套接字API（第三版）》

# 0x1 完成一次http连接

先把最基础的东西实现了，直接上代码然后慢慢看吧

```c
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

typedef int file_descriptor;

int main() {
    const int port = 80;
    char *host = "localhost";
    char *request = "GET http://10.10.10.138/ HTTP/1.1\r\n"
            "Host: 10.10.10.138\r\n"
            "Connection: keep-alive\r\n"
            "Cache-Control: max-age=0\r\n"
            "Upgrade-Insecure-Requests: 1\r\n"
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/65.0.3325.162 Safari/537.36\r\n"
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\n"
            "Accept-Encoding: gzip, deflate\r\n"
            "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n\r\n";
    int request_length = strlen(request);
    struct hostent *server;
    struct sockaddr_in serverAddr;
    char response[8192] = {};
    size_t total, sent, received = 0;
    ssize_t bytes;

    file_descriptor sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Failed Opening socket.\n");
        return 1;
    }

    server = gethostbyname(host);
    if (server == NULL) {
        printf("No Such Host.\n");
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);

    if(connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0){
        printf("Failed connecting.\n");
        return 1;
    }

    total = strlen(request);
    sent = 0;
    do {
        bytes = send(sock, request, request_lenth, 0);
        if (bytes < 0){
            printf("Failed writing message to socket\n");
            return 1;
        }
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);

    recv(sock, response, sizeof(response) - 1, 0);
    
    close(sockfd);

    printf("Response:\n%s\n",response);

    return 0;
}
```

现在先来分析一下这个简单的单次连接程序

首先HTTP请求的大致过程是：

- 浏览器与服务器建立一个**可靠**连接
- 浏览器发出请求(request)
- 服务器做出响应(response)
- 断开连接

所以如果用C来写的话，大概可以分为以下步骤：

- 创建一个类型为SOCK_STREAM的socket（原因之后分析）
- 建立与服务器的连接，如果没有服务器的具体信息，需要通过调用方法来获得
- 发出请求
- 等待响应
- 关闭socket

现在开始一步步把程序写出来

## 创建socket

```c
    file_descriptor sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Failed Opening socket.\n");
        return 1;
    }

```

创建连接的时候就需要调用Linux底层提供的socket函数，如果创建socket成功，就会返回一个最小的能用的文件描述符（一个无符号整数），否则返回负数表示失败。函数原型为

```c
int socket(int domain, int type, int protocol);
```

该函数有三个参数，第一个参数指定socket domain，用处是识别socket地址格式，另一个是确定通信范围，比如AF_UNIX用来作为本地通信，AF_INET表示在使用IPv4网络的主机间通信。这里用来本机测试的话，AF_UNIX或者AF_INET都行，不过后面考虑把测试用的虚拟机和架设服务器的虚拟机分开以求不互相影响，所以用了后者。

第二个参数指定socket类型，SOCK_STREAM类型提供面向连接的可靠数据传送，那这个不就是能达到一个TCP的嘛，而在《Linux/UNIX系统编程手册》P947上也有提到，“流socket（通常）采用了TCP”。因为http底层需要可靠数据连接服务，所以直接用SOCK_STREAM就行。同时Linux在内核2.6.27之后为这个参数增加了一个用途，允许两个标记与socket类型取或，来使得内核为返回的文件描述符启用close-on-exec或者使得该socket上发生的I/O变成非阻塞的。

第三个参数是用于确定协议族下的特定协议的，但是在通常大多数情况下，只需要留给它0就行了。

## 调用方法获得地址的信息

```c
    server = gethostbyname(host);
    if (server == NULL) {
        printf("No Such Host.\n");
        return 1;
    }

	bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
```

gethostbyname(char *)如函数名一样直接，该方法通过DNS、host文件来从主机名或者IPv4地址获得具体的信息，如果是IPv4地址，则不会使用DNS。函数若成功返回一个指向包含该服务器信息的hostent结构的指针，否则返回NULL。

这里还有一点值得一提，h(ost)to(net)s函数，做的就是字节序转换，因为网络通信常用大端序，而主机上又以小端序为主，所以如果亲自实现这些接口的话就需要注意这一点。

至于为什么后面四行要那样写，个人理解来就是根据函数的接口与结构体定义，类似一个八股文的形式了。

## 建立连接

```c
    if(connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0){
        printf("Failed connecting.\n");
        return 1;
    }
```

先放下这个函数的原型

``` c
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

第一个参数就是我们在第一步里面获得的文件描述符，第二个则是第二步里获得的地址信息，第三个参数是addr的大小。

记得之前说的吗，可以通过在socket()第二个参数socket使用或使socket上发生的I/O变成非阻塞的，但是如果那样做的话，在这里就会返回一个-1，并将errno（如果引入errno.h）置为115（EINPROGRESS），但是已经发起的握手其实是继续进行的，这里处理操作设计I/O复用，目前预计在第三篇中学习。

## 发起请求

```c
    total = strlen(request);
    sent = 0;
    do {
        bytes = send(sock, request, request_lenth, 0);
        if (bytes < 0){
            printf("Failed writing message to socket\n");
            return 1;
        }
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);
```

这个感觉也是个八股文的东西吧，就是尝试把请求发出去。请求内容我是直接访问了一次网站，抓了一次包，去掉了最后一个确定是否更改的标记。

对socket进行操作的时候，send和write两个的效果几乎是一样的，不同点在于send最后传入一个flag参数，用于提供诸如不使用网关等额外操作。此外send用在非I/O模式的socket上是非阻塞的，但是write总是会阻塞的。

## 等待响应

``` c
	do {
    	recv(sockfd, response, sizeof(response) - 1, 0);
        if (bytes < 0){
            printf("Failed writing message to socket\n");
            return 1;
        }
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);
```

一样的操作。

对socket进行操作时，recv与read两个的异同和send与write间的关系类似。

# 0x2 总结与挖掘

第一篇笔记，简单实现了C语言的http请求与响应。

在这篇中，我觉得还可以深挖的东西有：

- 文件描述符是什么？它的范围是怎么样的？对它的操作涉及什么？
- socket的类型中还有其他可以提供可靠连接的类型，是否也能用它实现http请求？

对于以上问题，目前有的答案是：

- 所有执行I/O操作的系统调用都以文件描述符来之指代一个打开的文件，此处的文件并非一个狭窄的概念，pipe、FIFO、socket、终端、设备和普通文件都被囊括在这一范围内，它被表示为一个非负整数（通常用可以使用的最小非负整数，所以这也解释了为什么错误处理的时候是检测返回文件描述符是否为负）。在Linux系统中，可以通过查看`cat /proc/sys/fs/file-max`来确定系统中能用的文件描述符总量，但是在一个进程中打开过多文件描述符会返回一个“too many open files”错误，对于单个进程的文件描述符数量上限可以使用`ulimit -a`查看，同时可以依照下篇笔记中的博客对Ubuntu12/14/16进行调整。对文件描述符的操作其实就是I/O操作。
- 这是一个需要实践的问题，我暂时还没有进行尝试。