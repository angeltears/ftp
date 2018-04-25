#include "ftpproto.h"
#include "sysutil.h"
#include "str.h"
#include "ftpcode.h"
#include <iostream>
using namespace std;


void do_user(session_t *sess);
static void do_pass(session_t *sess);
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
        //解析FTP命令与参数
        str_split(sess->cmdline, sess->cmd, sess->arg, ' ');
        //将命令转化成大写
        str_upper(sess->cmd);
        //处理内部命令
        if (strcmp(sess->cmd, "USER") == 0)
        {
            do_user(sess);
        }
        if (strcmp(sess->cmd, "PASS") == 0)
        {
            do_pass(sess);
        }
    }
    
}


void ftp_reply(session_t *sess, int status, const char *text)
{
    char buf[1024] = {0};
    sprintf(buf, "%d %s\r\n",status, text);
    writen(sess->ctrl_fd, buf, strlen(buf));
}


void do_user(session_t *sess)
{
    struct passwd *pw  = getpwnam(sess->arg);
    if (pw == NULL)
    {
        ftp_reply(sess, FTP_LOGINERR, "Login incorrect");
        return;
    }
    sess->uid = pw->pw_uid;
    ftp_reply(sess, FTP_GIVEPWORD, "Please specify the password.");
}

static void do_pass(session_t *sess)
{
    struct passwd *pw = getpwuid(sess->uid);
    if (pw == NULL)
    {
        ftp_reply(sess, FTP_LOGINERR, "Login incorrect");
        return;
    }
    struct spwd *sp = getspnam(pw->pw_name);
    if (sp == NULL)
    {
        ftp_reply(sess, FTP_LOGINERR, "Login incorrect");
        return;
    }
    char *encrypted_pw = crypt(sess->arg, sp->sp_pwdp);
    if (strcmp(encrypted_pw, sp->sp_pwdp) != 0)
    {
        ftp_reply(sess, FTP_LOGINERR, "Login incorrect.");
        return;
    }

    setegid(pw->pw_gid);
    seteuid(pw->pw_uid);
    chdir(pw->pw_dir);
    ftp_reply(sess, FTP_LOGINOK, "Login success.");

}