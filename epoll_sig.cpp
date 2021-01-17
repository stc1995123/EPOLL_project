#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <assert.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_EVENTS 1024
// 将fd 设置为非阻塞： get old_option, set new_option, fcntl();
int setnonblocking(int fd){ // F_GETFL, F_SETFL ;
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK; // 非阻塞置位;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
void prt();
void addfd( int epollfd, int fd, bool enable_et){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN; // LT
    if(enable_et){
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event); // 注册采用指令： EPOLL_CTL_ADD;
    setnonblocking(fd);
}

void lt(epoll_event* events, int number, int epollfd, int listenfd){ //注意这里的epoll_event指针
    char buffer[BUFFER_SIZE];
    
    //处理每个就绪的events[i];
    for(int i=0;i<number;i++){ 
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd){
            //  连接事件.
            sockaddr_in client_address;
            socklen_t len = sizeof(client_address); //因为后面accept用实参
            int connfd = accept(listenfd, (sockaddr*)& client_address, &len);
            addfd(epollfd, connfd, false);// 将connfd 加入epoll 监视注册表中。LT模式
        }
        else if(events[i].events & EPOLLIN){ //可读事件就绪 ：接受数据.
            printf("event trigger \n");
            memset(buffer, '\0', sizeof(buffer));
            int ret = recv(sockfd, buffer, BUFFER_SIZE-1, 0); //sockfd 是当前处理的就绪事件》
            if (ret<=0){ //连接关闭的标志.
                printf("close socket\n");
                close(sockfd);
                break;
            }
            printf("get %d bytes from client: %s\n",ret, buffer); //接受数据.
        }
        else{
            // 其他事件（不是连接， 也不是可读就绪，先不管》 
            printf("other thing\n");
        }
    }

}


int main(){

    // listen 
    const char* ip ="127.0.0.1";//host;
    prt();
    int port = 12345;
    int ret =0;
    sockaddr_in address; 
    address.sin_port = htons(port);
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    ret = bind(listenfd, (sockaddr*)& address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 4);
    assert(ret != -1);

    // epoll 
    epoll_event events[MAX_EVENTS];
    int epollfd = epoll_create(5);
    assert(epollfd!= -1);
    addfd(epollfd, listenfd, false); //listenfd 注册到 内核事件表》
    
    // epoll_wait();
    while(1){
        int ret = epoll_wait(epollfd, events, MAX_EVENTS, -1); // -1 是超时时间为无穷》
        if( ret<0 ){
            printf("epoll failed\n");
            break;

        }
        lt(events, ret, epollfd, listenfd); //ret 是epoll_wait()返回的就绪消息数目；listenfd 是为了检测连接事件。
    }
    close(listenfd);
    return 0;
}