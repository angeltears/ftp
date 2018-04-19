#include "sysutil.h"

/**
*getlocalip 获得本地ip地址, 返回值等于-1表示获取失败,返回等于0表示获取成功
*@ip  传出本地ip的地址
*/
int getlocalip(char *ip)
{
    char host[100] = {0};
    if (gethostname(host, sizeof(host))< 0)     //获得本地host的name
        return -1;
    struct hostent *hp;
/*
    struct hostent 
    { 
    char * h_name; / *主机的正式名称* / 
    char ** h_aliases; / *别名列表* / 
    int h_addrtype; / *主机地址类型* / 
    int h_length; / *地址长度* / 
    char ** h_addr_list; / *地址列表* / 
    };
    #define h_addr h_addr_list [0] 
*/
    if ((hp = gethostbyname(host)) == NULL)     //通过本地hostname获得主机信息
        return -1;
    
    strcpy(ip, inet_ntoa(*(struct in_addr *)hp->h_addr));
    return 0;
}

/**
 * activate_noblock - 设置I/O为非阻塞模式
 * @fd: 文件描符符
 */
void activate_nonblock(int fd)
{
    int ret;
    int flag = fcntl(fd, F_GETFL);  //获取文件权限
    if (flag == -1)
        ERR_EXIT("fcntl");
    
    flag |= O_NONBLOCK;
    ret = fcntl(fd, F_SETFL);     //设置文件权限
    if (ret == -1)
        ERR_EXIT("fcntl");
}

/**
 * deactivate_nonblock - 设置I/O为阻塞模式
 * @fd: 文件描符符
 */
void deactivate_nonblock(int fd)
{
    int ret;
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        ERR_EXIT("fcntl");
    
    flags &= ~O_NONBLOCK;
    ret = fcntl(fd, F_SETFL);
    if (ret == -1)
         ERR_EXIT("fcntl");
    
}

/**
 * read_timeout - 读超时检测函数，不含读操作
 * @fd: 文件描述符
 * @wait_seconds: 等待超时秒数，如果为0表示不检测超时
 * 成功（未超时）返回0，失败返回-1，超时返回-1并且errno = ETIMEDOUT
 * 使用的是select模型
 */
int read_timeout(int fd, unsigned int wait_seconds)
{
    int ret = 0;
    if (wait_seconds > 0)
    {    fd_set fds;
        struct timeval timeout;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeout.tv_sec = wait_seconds;
        timeout.tv_usec = 0;
        do
        {
            ret = select(fd + 1, &fds, NULL, NULL, &timeout);
        }while(ret == -1 && errno == EINTR);

		if (ret == 0) {
			ret = -1;
			errno = ETIMEDOUT;
		}
        if (ret == 1)
        {
            ret = 0;
        }
    }   
    return ret;
}
/**
 * write_timeout - 读超时检测函数，不含写操作
 * @fd: 文件描述符
 * @wait_seconds: 等待超时秒数，如果为0表示不检测超时
 * 成功（未超时）返回0，失败返回-1，超时返回-1并且errno = ETIMEDOUT
 */
int write_timeout(int fd, unsigned int wait_seconds)
{
        int ret = 0;
    if (wait_seconds > 0)
    {    fd_set fds;
        struct timeval timeout;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeout.tv_sec = wait_seconds;
        timeout.tv_usec = 0;
        do
        {
            ret = select(fd + 1, NULL, &fds, NULL,  &timeout);
        }while(ret == -1 && errno == EINTR);

		if (ret == 0) {
			ret = -1;
			errno = ETIMEDOUT;
		}
        if (ret == 1)
        {
            ret = 0;
        }
    }   
    return ret;
}
/**
 * accept_timeout - 带超时的accept
 * @fd: 套接字
 * @addr: 输出参数，返回对方地址
 * @wait_seconds: 等待超时秒数，如果为0表示正常模式
 * 成功（未超时）返回已连接套接字，超时返回-1并且errno = ETIMEDOUT
 */
int accept_timeout(int fd, struct sockaddr_in *addr, unsigned int wait_seconds)
{
    
}
int connect_timeout(int fd, struct sockaddr_in *addr, unsigned int wait_seconds);

ssize_t readn(int fd, void* buf, size_t count);
ssize_t waiten(int fd, void* buf, size_t count);
ssize_t recv_peek(int sockfd, void *buf, size_t len);
ssize_t readline(int sockfd, void *buf, size_t maxline);
void send_fd(int sock_fd, int fd);
int recv_fd(const int sock_fd);
