#include "common.h"
#include "sysutil.h"
#include "ftpproto.h"
#include "ftpcode.h"
#include <iostream>
#include "tunable.h"
#include "parseconf.h"
#include "hash.h"
/*


typedef struct session_t
{
    // 控制链接
    uid_t uid;
    int ctrl_fd;
    char cmdline[MAX_COMMAND_LINE];
    char cmd[MAX_COMMAND];
    char arg[MAX_ARG];
    // 数据连接
    struct sockaddr_in *port_addr;
    int pasv_listen_fd;
    int data_fd;
    int data_process;

    //限速
    unsigned int bw_upload_rate_max;
    unsigned int bw_download_rate_max;
    long bw_transfer_start_sec;
    long bw_transfer_start_usec;

    //父子进程通道
    int parent_fd;
    int child_fd;

    // FTP协议状态
    bool is_ascii;
    long long restart_pos;
    char *rnfr_name;
    int abor_received;

    //链接数限制
    unsigned int num_clients;
    unsigned int num_this_ip;
}session_t ;
 */
static unsigned int s_children;
static hash* s_ip_conn_hash;
static hash* s_pid_ip_hash;
void check_limits(session_t *sess);
unsigned int hash_func(unsigned int buckets, void *key);
void handle_sighid(int sig);
unsigned int handle_ip_count(void *ip);
void drop_ip_count(void *ip);

int main()
{
    parseconf_load_file(MINIFPT_CONFIG);
    daemon(0, 0);
    printf("tunable_pasv_enable=%d\n", tunable_pasv_enable);
    printf("tunable_port_enable=%d\n", tunable_port_enable);
    printf("tunable_listen_port=%u\n", tunable_listen_port);
    printf("tunable_max_clients=%u\n", tunable_max_clients);
    printf("tunable_max_per_ip=%u\n", tunable_max_per_ip);
    printf("tunable_accept_timeout=%u\n", tunable_accept_timeout);
    printf("tunable_connect_timeout=%u\n", tunable_connect_timeout);
    printf("tunable_idle_session_timeout=%u\n", tunable_idle_session_timeout);
    printf("tunable_data_connection_timeout=%u\n", tunable_data_connection_timeout);
    printf("tunable_local_umask=0%o\n", tunable_local_umask);
    printf("tunable_upload_max_rate=%u\n", tunable_upload_max_rate);
    printf("tunable_download_max_rate=%u\n", tunable_download_max_rate);
    printf("tunable_listen_address=%s\n", tunable_listen_address);


    if (getuid() != 0)
    {
        fprintf(stderr, "miniftp must start be as root\n");
        exit(EXIT_FAILURE);
    }

    session_t sess =
    {
         /*控制链接*/
         0,-1,"","","",
         /*数据连接*/
         NULL,-1,-1,0,
         /*限速*/
         0,0 ,0,0
        /*父子进程通道*/
        -1,-1,
         /*FTP协议状态*/
         0,NULL,0,0,
         /*最大连接数*/
         0,0
    };
    p_sess = &sess;
    sess.bw_upload_rate_max = tunable_upload_max_rate;
    sess.bw_download_rate_max = tunable_download_max_rate;
    s_ip_conn_hash = new hash(256, hash_func);
    s_pid_ip_hash = new hash(256, hash_func);
    signal(SIGCHLD, handle_sighid);
    int listenfd = tcp_server(NULL, 5188);
    int conn;
    pid_t pid;
    struct sockaddr_in addr;
    while(1)
    {
        conn = accept_timeout(listenfd, &addr, 0);
        if (conn == -1)
        {
             ERR_EXIT("accpet_timeout");
        }
        unsigned int ip = addr.sin_addr.s_addr;
        sess.num_this_ip = handle_ip_count(&ip);
        ++s_children;
        sess.num_clients = s_children;
        pid = fork();
        if (pid == -1)
        {
            --s_children;
            ERR_EXIT("fork");
        }
        if (pid == 0)    //子进程
        {
            close(listenfd);
            sess.ctrl_fd = conn;
            check_limits(&sess);
            signal(SIGCHLD, SIG_IGN);
            begin_session(&sess);
        }
        else            //父进程
        {
            s_pid_ip_hash->hash_add_entry(&pid, sizeof(pid), &ip, sizeof(ip));
            close(conn);
        }
     }
    return 0;
 }

void check_limits(session_t *sess)
{
    if (tunable_max_clients > 0 && sess->num_clients > tunable_max_clients)
    {
        ftp_reply(sess, FTP_TOO_MANY_USERS, "There are too many connection, please try later");
        exit(EXIT_FAILURE);
    }
    if (tunable_max_per_ip > 0 && sess->num_this_ip > tunable_max_per_ip)
    {
        ftp_reply(sess, FTP_IP_LIMIT, "There are too many connection,from internet address");
        exit(EXIT_FAILURE);
    }
}

void handle_sighid(int sig)
{
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        --s_children;
        unsigned int *ip = (unsigned  int *)s_pid_ip_hash->hash_lookup_entry(&pid, sizeof(pid));
        if (ip == NULL)
        {
            continue;
        }
        drop_ip_count(ip);
        s_pid_ip_hash->hash_free_entry(&pid, sizeof(pid));
    }
}
unsigned int hash_func(unsigned int buckets, void *key)
{
    unsigned  int * num = (unsigned int *)key;
    return (*num) % buckets;
}

unsigned int handle_ip_count(void *ip)
{
    unsigned int count;
    unsigned int *p_count = (unsigned  int *)s_ip_conn_hash->hash_lookup_entry(ip, sizeof(unsigned int));
    if (p_count == NULL)
    {
        count = 1;
        s_ip_conn_hash->hash_add_entry(ip, sizeof(unsigned int), &count, sizeof(unsigned int));
    }
    else
    {
        count = *p_count;
        ++count;
        *p_count = count;
    }
    return count;
}

void drop_ip_count(void *ip)
{
    unsigned int count;
    unsigned int *p_count = (unsigned  int *)s_ip_conn_hash->hash_lookup_entry(ip, sizeof(unsigned int));
    if (p_count == NULL)
    {
       return;
    }

    count = *p_count;
    if (count <= 0)
    {
        s_ip_conn_hash->hash_free_entry(ip, sizeof(unsigned int));
        return;
    }
    --count;
    *p_count = count;

    if (count == 0)
    {
        s_ip_conn_hash->hash_free_entry(ip, sizeof(unsigned int));
    }
}
