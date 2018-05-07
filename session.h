#ifndef _SESSION_H_
#define _SESSION_H_
#include "common.h"


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
extern session_t *p_sess;

void begin_session(session_t  * sess);

#endif