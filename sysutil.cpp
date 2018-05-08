#include "sysutil.h"
#include <ifaddrs.h>

/**
 * tcp_client -用来创建一个tcp服务器，指定一个端口号
 * @port : 服务器的端口号，大于０则指定一个端口号，等于０则为随机分配一个端口号
 * 成功返回监听套接字
 */

int tcp_client(unsigned short port)
{
    int sock;
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        ERR_EXIT("TCP_CLIENT");
    }

    if (port > 0)
    {
        int on = 1;
        if(setsockopt(sock, SOL_SOCKET,SO_REUSEADDR, (const char*)&on, sizeof(on))< 0)
        {
            ERR_EXIT("setsockopt");
        }
        char ip[15] = {0};
        getlocalip(ip);
        struct sockaddr_in localaddr;
        memset(&localaddr, 0, sizeof(localaddr));
        localaddr.sin_family = AF_INET;
        localaddr.sin_port = htons(port);
        localaddr.sin_addr.s_addr = inet_addr(ip);
        if((bind(sock, (struct sockaddr*)&localaddr, sizeof(localaddr))) < 0)
        {
            ERR_EXIT("bind");
        }
    }
    return sock;
}


/**
 * tcp_server -用来启动一个tcp服务器
 * @host : 服务器主机名或者是ip地址
 * @port : 服务器的端口号
 * 成功返回监听套接字
 */
int tcp_server(const char *host, unsigned short port)
{
    int listenfd;
    if ((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        ERR_EXIT("TCP_SERVER");
    }
    
    struct sockaddr_in seraddr;
    memset(&seraddr, 0, sizeof(seraddr));
    seraddr.sin_family = AF_INET;
    if(host != NULL)
    {
        if (inet_aton(host, &seraddr.sin_addr) == 0)
        {
            struct hostent *hp;
            if((hp = gethostbyname(host)) == NULL)
            {
                ERR_EXIT("gethostbyname");
            }
            seraddr.sin_addr  = *(struct in_addr*)hp->h_addr;
        }
    }
    else
    {
        seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    seraddr.sin_port = htons(port);
    int on = 1;
    if(setsockopt(listenfd, SOL_SOCKET,SO_REUSEADDR, (const char*)&on, sizeof(on))< 0)
    {
        ERR_EXIT("setsockopt");
    }
    if((bind(listenfd, (struct sockaddr*)&seraddr, sizeof(seraddr))) < 0)
    {
        ERR_EXIT("bind");
    }
    if((listen(listenfd, SOMAXCONN)) < 0)
    {
        ERR_EXIT("listen");
    }

    return listenfd;
}


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
 * 采用io复用的select模型
 */
int accept_timeout(int fd, struct sockaddr_in *addr, unsigned int wait_seconds)
{
    int ret;
    socklen_t socklen = sizeof(struct sockaddr_in);

    if (wait_seconds > 0)
    {
        fd_set accpfds;
        struct timeval waittime;

        FD_ZERO(&accpfds);
        FD_SET(fd, &accpfds);

        waittime.tv_sec = wait_seconds;
        waittime.tv_usec = 0;

        do
        {
            ret = select(fd + 1, &accpfds, NULL, NULL, &waittime);
        }while(ret < 0 && errno == EINTR);

        if (ret == 0)
        {
            errno = ETIMEDOUT;
            return -1;
        }
   }

    if(addr == NULL)
    {
        ret = accept(fd, NULL, NULL);
    }
    else
    {
        ret = accept(fd, (struct sockaddr*)addr, &socklen);
    }
    return ret;
}


/**
 * connect_timeout - connect
 * @fd: 套接字
 * @addr: 要连接的对方地址
 * @wait_seconds: 等待超时秒数，如果为0表示正常模式
 * 成功（未超时）返回0，失败返回-1，超时返回-1并且errno = ETIMEDOUT
 * 采用io复用的select模型
 */
int connect_timeout(int fd, struct sockaddr_in *addr, unsigned int wait_seconds)
{
    int ret;
    socklen_t socklen = sizeof(struct sockaddr_in);

    if (wait_seconds > 0)
        activate_nonblock(fd);
    
    ret = connect(fd, (struct sockaddr *)addr, socklen);
    if (ret < 0 && errno == EINPROGRESS)
    {
        fd_set connfds;
        struct timeval timeout;
        
        FD_ZERO(&connfds);
        FD_SET(fd, &connfds);

        timeout.tv_sec = wait_seconds;
        timeout.tv_usec = 0;
        do
        {
            ret = select(fd + 1, NULL, &connfds, NULL, &timeout);
        }while(ret < 0 && errno == EINTR);

        if (ret == 0)
        {
            errno = ETIMEDOUT;
            return -1;
        }
        else if (ret < 0)
        {
            return -1;
        }
        else if(ret == 1)
        {
            /* ret返回为1，可能有两种情况，一种是连接建立成功，一种是套接字产生错误，*/
			/* 此时错误信息不会保存至errno变量中，因此，需要调用getsockopt来获取。 */
            int err;
            socklen_t len = sizeof(err);
            int sockopt = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
            if (sockopt == -1)
            {
                return -1;
            }
            if (err == 0)
            {
                ret = 0;
            }
            else
            {
                ret = -1;
                errno = err;
            }
        }
    }
    if (wait_seconds > 0)
    {
        deactivate_nonblock(fd);
    }
    return ret;
}

/**
 * readn - 读取固定字节数
 * @fd: 文件描述符
 * @buf: 接收缓冲区
 * @count: 要读取的字节数
 * 成功返回count，失败返回-1，读到EOF返回<count
 */
ssize_t readn(int fd, void* buf, size_t count)
{
    ssize_t nleft = count;
    ssize_t nread;
    char *bufp = (char *)buf;
    while(nleft > 0)
    {
        nread = read(fd, bufp, nleft);
        if (nread < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        else if (nread == 0)
        {
            return count - nleft;
        }

        nleft -= nread;
        bufp += nread;
    }
    return count;
}

/**
 * writen - 发送固定字节数
 * @fd: 文件描述符
 * @buf: 发送缓冲区
 * @count: 要读取的字节数
 * 成功返回count，失败返回-1
 */
ssize_t writen(int fd, const void* buf, size_t count)
{
    ssize_t nleft = count;
    ssize_t nwriten;
    char *bufp = (char *)buf;
    while(nleft > 0)
    {
        nwriten = write(fd, buf, nleft);
        if (nwriten < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if (nwriten == 0)
        {
            return count - nleft;
        }

        nleft -= nwriten;
        bufp += nwriten;
    }
    return count;
}

/**
 * recv_peek - 仅仅查看套接字缓冲区数据，但不移除数据
 * @sockfd: 套接字
 * @buf: 接收缓冲区
 * @len: 长度
 * 成功返回>=0，失败返回-1
 */
ssize_t recv_peek(int sockfd, void *buf, size_t len)
{
	while (1) 
    {
		int ret = recv(sockfd, buf, len, MSG_PEEK);
		if (ret == -1 && errno == EINTR)
			continue;
		return ret;
	}
}

/**
 * readline - 按行读取数据
 * @sockfd: 套接字
 * @buf: 接收缓冲区
 * @maxline: 每行最大长度
 * 成功返回>=0，失败返回-1
 */
ssize_t readline(int sockfd, void *buf, size_t maxline)
{
	int ret;
	int nread;
	char *bufp = (char *)buf;
	int nleft = maxline;
	while (1) 
    {
		ret = recv_peek(sockfd, bufp, nleft);
		if (ret < 0)
			return ret;
		else if (ret == 0)
			return ret;

		nread = ret;
		int i;
		for (i=0; i<nread; i++) 
        {
			if (bufp[i] == '\n') 
            {
				ret = readn(sockfd, bufp, i+1);
				if (ret != i+1)
					exit(EXIT_FAILURE);

				return ret;
			}
		}

		if (nread > nleft)
			exit(EXIT_FAILURE);

		nleft -= nread;
		ret = readn(sockfd, bufp, nread);
		if (ret != nread)
			exit(EXIT_FAILURE);
		bufp += nread;
	}

	return -1;
}

/**
 * send_fd -向sock_fd 发送 fd
 * @sock_fd: 发送目标套接字
 * @fd: 发送套接字
 */
void send_fd(int sock_fd, int fd)
{
    int ret;
    struct msghdr msg;
    struct cmsghdr *p_cmsg;
    struct iovec vec;
    char cmsgbuf[CMSG_SPACE(sizeof(fd))];
    int *p_fds;
    char sendchar = 0;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    p_cmsg = CMSG_FIRSTHDR(&msg);
	p_cmsg->cmsg_level = SOL_SOCKET;
	p_cmsg->cmsg_type = SCM_RIGHTS;
	p_cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	p_fds = (int*)CMSG_DATA(p_cmsg);
	*p_fds = fd;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;

	vec.iov_base = &sendchar;
	vec.iov_len = sizeof(sendchar);
	ret = sendmsg(sock_fd, &msg, 0);
	if (ret != 1)
		ERR_EXIT("sendmsg");
}

/**
 * send_fd -向sock_fd 发送 fd
 * @sock_fd: 接受目标套接字
 * 返回目标套接字
 */
int recv_fd(const int sock_fd)
{
    int ret;
	struct msghdr msg;
	char recvchar;
	struct iovec vec;
	int recv_fd;
	char cmsgbuf[CMSG_SPACE(sizeof(recv_fd))];
	struct cmsghdr *p_cmsg;
	int *p_fd;
	vec.iov_base = &recvchar;
	vec.iov_len = sizeof(recvchar);
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	msg.msg_flags = 0;

	p_fd = (int*)CMSG_DATA(CMSG_FIRSTHDR(&msg));
	*p_fd = -1;  
	ret = recvmsg(sock_fd, &msg, 0);
	if (ret != 1)
		ERR_EXIT("recvmsg");

	p_cmsg = CMSG_FIRSTHDR(&msg);
	if (p_cmsg == NULL)
		ERR_EXIT("no passed fd");


	p_fd = (int*)CMSG_DATA(p_cmsg);
	recv_fd = *p_fd;
	if (recv_fd == -1)
		ERR_EXIT("no passed fd");

	return recv_fd;
}

const char *statbuf_get_perms(struct stat *sbuf)
{
    static char power[] = "----------";
    power[0] = '?';

    mode_t mode = sbuf->st_mode;
    switch (mode & S_IFMT)
    {
        case S_IFREG:
            power[0] = '-';
            break;
        case S_IFDIR:
            power[0] = 'd';
            break;
        case S_IFLNK:
            power[0] = 'l';
            break;
        case S_IFSOCK:
            power[0] = 's';
            break;
        case S_IFIFO:
            power[0] = 'p';
            break;
        case S_IFBLK:
            power[0] = 'b';
            break;
        case S_IFCHR:
            power[0] = 'C';
            break;
    }
    if (mode & S_IRUSR)
    {
        power[1] = 'r';
    }
    if (mode & S_IWUSR)
    {
        power[2] = 'w';
    }
    if (mode & S_IXUSR)
    {
        power[3] = 'x';
    }
    if (mode & S_IRGRP)
    {
        power[4] = 'r';
    }
    if (mode & S_IWGRP)
    {
        power[5] = 'w';
    }
    if (mode & S_IXGRP)
    {
        power[6] = 'x';
    }
    if (mode & S_IROTH)
    {
        power[7] = 'r';
    }
    if (mode & S_IWOTH)
    {
        power[8] = 'w';
    }
    if (mode & S_IXOTH)
    {
        power[9] = 'x';
    }
    if (mode & S_ISUID)
    {
        power[3] = (power[3] == 'x') ? 's':'S';
    }
    if (mode & S_ISGID)
    {
        power[6] = (power[6] == 'x') ? 's':'S';
    }
    if (mode & S_ISVTX)
    {
        power[9] = (power[9] == 'x') ? 's':'S';
    }
    return power;
}

const char *stabuf_get_date(struct stat *sbuf)
{
    const char *p_data_format = "%b %e %H:%M";
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t local_time = tv.tv_sec;
    if(sbuf->st_mtime > local_time || (local_time - sbuf->st_mtime) > 182 * 24 * 60 *60);
    {
        p_data_format = "%b %e %Y";
    }

    static char databuf[64] = {0};
    struct tm * p_tm = localtime(&local_time);
    strftime(databuf, sizeof(databuf), p_data_format, p_tm);
    return databuf;
}

int lock_internal(int fd, int lock_type)
{
    int ret;
    struct flock the_lock;
    memset(&the_lock, 0, sizeof(the_lock));
    the_lock.l_type = lock_type;
    the_lock.l_whence = SEEK_SET;
    the_lock.l_start = 0;
    the_lock.l_len = 0;
    do
    {
        ret = fcntl(fd, F_SETLKW, &the_lock);
    } while (ret < 0 && errno == EINTR);

    return ret;
}
int lock_file_read(int fd)
{
    return lock_internal(fd, F_RDLCK);
}

int lock_file_write(int fd)
{
    return lock_internal(fd, F_WRLCK);
}
int unlock_file(int fd)
{
    int ret;
    struct flock the_lock;
    memset(&the_lock, 0, sizeof(the_lock));
    the_lock.l_type = F_UNLCK;
    the_lock.l_whence = SEEK_SET;
    the_lock.l_start = 0;
    the_lock.l_len = 0;
    ret = fcntl(fd, F_SETLKW, &the_lock);
    return ret;
}

static struct timeval s_curr_time;
long get_time_sec(void)
{
    if (gettimeofday(&s_curr_time, NULL) < 0)
    {
        ERR_EXIT("gettimeofday");
    }
    return s_curr_time.tv_sec;
}
long get_time_usec(void)
{
    return s_curr_time.tv_usec;
}
void nano_sleep(double seconds)
{
    time_t sesc = (time_t)seconds;
    double fractional = seconds - (double)sesc;

    struct timespec ts;
    ts.tv_sec =sesc;
    ts.tv_nsec = (long)(fractional * (double) 1000000000);

    int ret;
    do
    {
        ret = nanosleep(&ts, &ts);
    }
    while(ret == -1 && errno == EINTR);
}


//开启套接字fd接受带外数据功能
void activate_oobinline(int fd)
{
    int oob_inline = 1;
    int ret;
    ret = setsockopt(fd, SOL_SOCKET, SO_OOBINLINE, &oob_inline, sizeof(oob_inline));
    if (ret == -1)
    {
        ERR_EXIT("setsockopt");
    }
}

//当文件描述符fd上有带外数据时候，将产生sigurg信号
//该函数设定当前信号能够接受fd文件描述符所产生的sigurg信号
void activate_sigurg(int fd)
{
    int ret;
    ret = fcntl(fd, F_SETOWN, getpid());
    if (ret == -1)
    {
        ERR_EXIT("fcntl");
    }
}