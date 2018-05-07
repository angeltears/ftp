#include "common.h"
#include "sysutil.h"
#include "session.h"
#include "ftpproto.h"
#include <iostream>
#include "tunable.h"      
#include "parseconf.h"


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
}session_t ;

 */



int main()
{
    parseconf_load_file(MINIFPT_CONFIG);

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
         0,0,NULL
    };
    p_sess = &sess;
    sess.bw_upload_rate_max = tunable_upload_max_rate;
    sess.bw_download_rate_max = tunable_download_max_rate;
    signal(SIGCHLD, SIG_IGN);
    int listenfd = tcp_server(NULL, 5188);
    int conn;
    pid_t pid;
    while(1)
    {
      conn = accept_timeout(listenfd, NULL, 0);
      if (conn == -1)
      {
          ERR_EXIT("accpet_timeout");
      }
      pid = fork();
      if (pid == -1)
         ERR_EXIT("fork");

      if (pid == 0)    //子进程
      {
          close(listenfd);
          sess.ctrl_fd = conn;
          begin_session(&sess);
      }
      else            //父进程
      {
          close(conn);
      }
     }
    return 0;
 }
