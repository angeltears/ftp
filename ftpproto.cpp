#include "ftpproto.h"
#include "sysutil.h"
#include "str.h"
#include <iostream>
using namespace std;
void handle_child(session_t *sess)
{  
    writen(sess->ctrl_fd, (void *)"220 (minifpt 0.1)\r\n", strlen("220 (minifpt 0.1)\r\n"));
    int ret;
    while(1)
    {
        memset(sess->cmdline, 0, sizeof(sess->cmdline));
        memset(sess->cmd, 0, sizeof(sess->cmd));
        memset(sess->arg, 0, sizeof(sess->arg));
        ret = readline(sess->ctrl_fd, sess->cmdline, MAX_COMMAND_LINE);
        if (ret == 1)
            ERR_EXIT("readline");
        else if(ret == 0)
            exit(EXIT_SUCCESS);
        str_trim_crlf(sess->cmdline);
        printf("cmdline=[%s]\n",sess->cmdline);
        //解析FTP命令与参数
        //处理内部命令
        str_split(sess->cmdline, sess->cmd, sess->arg, ' ');
        printf("cmd = [%s], arg = [%s]", sess->cmd, sess->arg );
    }
    
}