#include "common.h"
#include "sysutil.h"
#include "session.h"
#include <iostream>
#include "tunable.h"      
#include "parseconf.h"


/* typedef struct session
{
    uid_t uid;
    // 控制链接
    char cmdline[MAX_COMMAND_LINE];
    char cmd[MAX_COMMAND];
    char arg[MAX_ARG];
    //父子进程通道
    int parent_fd;
    int child_fd;
    // FTP协议状态
    bool is_ascii;
}session; */

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
        /*父子进程通道*/
        -1,-1,
         /*FTP协议状态*/
         0
    };
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
