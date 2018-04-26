#ifndef _SESSION_H_
#define _SESSION_H_
#include "common.h"


typedef struct session_t
{
    uid_t uid;
    // 控制链接
    int ctrl_fd;
    char cmdline[MAX_COMMAND_LINE];
    char cmd[MAX_COMMAND];
    char arg[MAX_ARG];
    //父子进程通道
    int parent_fd;
    int child_fd;
    // FTP协议状态
    bool is_ascii;
}session_t ;


void begin_session(session_t  * sess);

#endif