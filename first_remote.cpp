#include <iostream>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "thread_pool.h"

#define PORT 2048
#define MAX_EVENT 20

int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main()
{
    int server_fd = socket(AF_INET,SOCK_STREAM,0);
    /*
    AF_INET : IPV4 <--> AF_INET6:IPV6
    SOCK_STREAM : TCP <--> SOCK_DGRAM : UDP
    0 : 默认协议
    */
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    /*
    struct sockaddr_in {
    sa_family_t    sin_family;   // 地址族 (AF_INET)
    uint16_t      sin_port;     // 端口号 (网络字节序)
    struct in_addr sin_addr;    // IP 地址
    char          sin_zero[8];  // 填充字段，保持结构体大小一致
    };
    */
    bind(server_fd,(sockaddr*)&addr,sizeof(addr));

    std::cout<<"server created success\n";
    listen(server_fd,10);
    setNonBlocking(server_fd);

    int epoll_fd = epoll_create1(0);
    if(epoll_fd==-1){
        std::cout<<"create epoll_fd failed !\n";
        return 0;
    }

    epoll_event ev{}, events[MAX_EVENT];
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    /*
    struct epoll_event {
    uint32_t events;   // 事件类型 (EPOLLIN, EPOLLOUT, EPOLLERR 等)
    epoll_data_t data; // 用户自定义数据 (通常存储文件描述符)
    };
    而：
    typedef union epoll_data {
    void        *ptr;    // 用户自定义指针
    int          fd;     // 文件描述符
    uint32_t     u32;    // 无符号 32 位整数
    uint64_t     u64;    // 无符号 64 位整数
    } epoll_data_t;
    */
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_fd,&ev);
    /*
    int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
    */

    ThreadPool threads(8);
    std::cout<<"start epoll!\n";
    while(1){
        int n_fds = epoll_wait(epoll_fd,events,MAX_EVENT,-1);

        for(int i=0;i<n_fds;++i){
            if(events[i].events & EPOLLIN){

                if(events->data.fd==server_fd){
                    int client_fd = accept(server_fd,nullptr,nullptr);
                    setNonBlocking(client_fd);

                    epoll_event client_ev{};
                    client_ev.events = EPOLLIN | EPOLLET;
                    client_ev.data.fd = client_fd;
                    
                    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&client_ev);
                    std::cout<<"new client connected !\n";
                }
                else{
                    threads.add_task([events,i](){
                        char buffer[1024]={0};
                        int client_fd = events[i].data.fd;
                        int len = read(client_fd,buffer,sizeof(buffer));
                        if(len==0){
                            close(client_fd);
                            std::cout<<"client:"<<client_fd<<" disconnected in thread:"<<std::this_thread::get_id();
                        }
                        else{
                            std::cout<<"thread:"<<std::this_thread::get_id()<<"received message:"<< buffer<<std::endl;
                            write(client_fd,buffer,len);
                        }
                    });
                }
            }
        }
    }
    return 0;
}