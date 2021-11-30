# 处理HTTP请求

## epoll
```c++
int epoll_create(int size)
```
创建一个指示epoll内核事件表的文件描述符，该描述符将用作其他epoll系统调用的第一个参数，size不起作用。

```c++
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
```
用于操作内核事件表监控的文件描述符上的事件：注册、修改、删除。

```c++
struct epoll_event {
    __uint32_t events; /* Epoll events */
    epoll_data_t data; /* User data variable */
};
```
EPOLLIN：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）\
EPOLLOUT：表示对应的文件描述符可以写\
EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）\
EPOLLERR：表示对应的文件描述符发生错误\
EPOLLHUP：表示对应的文件描述符被挂断\
EPOLLET：将EPOLL设为边缘触发（Edge Triggered）模式，这是相对于水平触发（Level Triggered）而言的\
EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里，防止多个线程处理一个socket

```c++
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
```
用于等待所监控文件描述符上有事件的产生，返回就绪的文件描述符个数。


## HTTP报文

### 请求报文
HTTP请求报文由请求行（request line）、请求头部（header）、空行和请求数据四个部分组成。
```
请求方法 URL 协议版本\r\n （请求行）
头部字段名:值\r\n（请求头部）
...
头部字段名:值\r\n
\r\n
xxxxx （请求数据）
```

### 响应报文
HTTP响应也由四个部分组成，分别是：状态行、消息报头、空行和响应正文。
```
协议版本 状态码 状态码描述\r\n （状态行）
头部字段名:值\r\n（消息报头）
...
头部字段名:值\r\n
\r\n
xxxxx （响应正文）
```

### 报文处理流程
+ 浏览器端发出http连接请求，主线程创建http对象接收请求并将所有数据读入对应buffer，将该对象插入任务队列，工作线程从任务队列中取出一个任务进行处理。
+ 工作线程取出任务后，调用process_read函数，通过主、从状态机对请求报文进行解析。
+ 解析完之后，跳转do_request函数生成响应报文，通过process_write写入buffer，返回给浏览器端。