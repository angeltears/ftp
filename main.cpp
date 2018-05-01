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
    //父子进程通道
    int parent_fd;
    int child_fd;
    // FTP协议状态
    bool is_ascii;
    long long restart_pos;
}session_t ;
 */

int main()
{
    parseconf_load_file(MINIFPT_CONFIG);
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
         NULL,-1,-1,
        /*父子进程通道*/
        -1,-1,
         /*FTP协议状态*/
         0,0,NULL
    };

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
