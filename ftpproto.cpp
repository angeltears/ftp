#include "ftpproto.h"
#include "sysutil.h"
#include "str.h"
#include "ftpcode.h"
#include "tunable.h"
#include "privsock.h"

void ftp_lreply(session_t *sess, int status, const char *text);

void handle_alarm_timeout(int sig);
void handle_sigalrm(int sig);
void handle_sigurg(int sig);
void start_cmdio_alarm(void);
void start_data_alarm(void);

void check_abor(session_t *sess);

int list_common(session_t *sess, int detail);
void limit_rate(session_t *sess, int bytes_transfered, int is_upload);
static void upload_common(session_t *sess, int is_append);

int get_port_fd(session_t *sess);
int get_pasv_fd(session_t *sess);
int get_transfer_fd(session_t *sess);
int port_active(session_t *sess);
int pasv_active(session_t *sess);

static void do_user(session_t *sess);
static void do_pass(session_t *sess);
static void do_cwd(session_t *sess);
static void do_cdup(session_t *sess);
static void do_quit(session_t *sess);
static void do_port(session_t *sess);
static void do_pasv(session_t *sess);
static void do_type(session_t *sess);
//static void do_stru(session_t *sess);
//static void do_mode(session_t *sess);
static void do_retr(session_t *sess);
static void do_stor(session_t *sess);
static void do_appe(session_t *sess);
static void do_list(session_t *sess);
static void do_nlst(session_t *sess);
static void do_rest(session_t *sess);
static void do_abor(session_t *sess);
static void do_pwd(session_t *sess);
static void do_mkd(session_t *sess);
static void do_rmd(session_t *sess);
static void do_dele(session_t *sess);
static void do_rnfr(session_t *sess);
static void do_rnto(session_t *sess);
static void do_site(session_t *sess);
static void do_syst(session_t *sess);
static void do_feat(session_t *sess);
static void do_size(session_t *sess);
static void do_stat(session_t *sess);
static void do_noop(session_t *sess);
static void do_help(session_t *sess);

static void do_site_chmod(session_t *sess, char *chmod_arg);
static void do_site_umask(session_t *sess, char *umask_arg);

typedef struct ftpcmd
{
    const char * cmd;
    void (*cmdhandler)(session_t *sess);
}ftpcmd_t;

static ftpcmd_t ctrl_cmds[] = {
        /* 访问控制命令 */
        {"USER",	do_user	},
        {"PASS",	do_pass	},
        {"CWD",		do_cwd	},
        {"XCWD",	do_cwd	},
        {"CDUP",	do_cdup	},
        {"XCUP",	do_cdup	},
        {"QUIT",	do_quit	},
        {"ACCT",	NULL	},
        {"SMNT",	NULL	},
        {"REIN",	NULL	},
        /* 传输参数命令 */
        {"PORT",	do_port	},
        {"PASV",	do_pasv	},
        {"TYPE",	do_type	},
        {"STRU",	/*do_stru*/NULL	},
        {"MODE",	/*do_mode*/NULL	},

        /* 服务命令 */
        {"RETR",	do_retr	},
        {"STOR",	do_stor	},
        {"APPE",	do_appe	},
        {"LIST",	do_list	},
        {"NLST",	do_nlst	},
        {"REST",	do_rest	},
        {"ABOR",	do_abor	},
        {"\377\364\377\362ABOR", do_abor},
        {"PWD",		do_pwd	},
        {"XPWD",	do_pwd	},
        {"MKD",		do_mkd	},
        {"XMKD",	do_mkd	},
        {"RMD",		do_rmd	},
        {"XRMD",	do_rmd	},
        {"DELE",	do_dele	},
        {"RNFR",	do_rnfr	},
        {"RNTO",	do_rnto	},
        {"SITE",	do_site	},
        {"SYST",	do_syst	},
        {"FEAT",	do_feat },
        {"SIZE",	do_size	},
        {"STAT",	do_stat	},
        {"NOOP",	do_noop	},
        {"HELP",	do_help	},
        {"STOU",	NULL	},
        {"ALLO",	NULL	}
};

session_t *p_sess;
void handle_alarm_timeout(int sig)
{
    shutdown(p_sess->ctrl_fd, SHUT_RD);
    ftp_reply(p_sess, FTP_IDLE_TIMEOUT, "Timeout");
    shutdown(p_sess->ctrl_fd, SHUT_WR);
    exit(EXIT_FAILURE);
}
void handle_sigalrm(int sig)
{
    if (!p_sess->data_process)
    {
        ftp_reply(p_sess, FTP_DATA_TIMEOUT, "Data timeout, Reconnect sorry");
        exit(EXIT_FAILURE);
    }

    //否则　当前处于数据传输的状态收到了超时信号
    p_sess->data_process = 0;
    start_data_alarm();
}
void handle_sigurg(int sig)
{
    if (p_sess->data_fd == -1)
    {
        return;
    }

    char cmdline[MAX_COMMAND_LINE] = {0};
    int ret = readline(p_sess->ctrl_fd, cmdline, MAX_COMMAND_LINE);
    if (ret <= 0)
    {
        ERR_EXIT("readline");
    }
    printf("cmdline = %s", cmdline);
    str_trim_crlf(cmdline);

    if (strcmp(cmdline, "ABOR") == 0
        || strcmp(cmdline, "\377\364\377\362ABOR") == 0)
    {
        p_sess->abor_received = 1;
        shutdown(p_sess->data_fd, SHUT_RDWR);
    }
    else
    {
        ftp_reply(p_sess, FTP_BADCMD, "Unknown command.");
    }
}
void start_cmdio_alarm()
{
    if (tunable_idle_session_timeout > 0)
    {
        //安装信号
        signal(SIGALRM, handle_alarm_timeout);
        //启动闹钟
        alarm(tunable_idle_session_timeout);
    }
}
void start_data_alarm()
{
    if (tunable_data_connection_timeout > 0)
    {
        //安装信号
        signal(SIGALRM, handle_sigalrm);
        //启动闹钟
        alarm(tunable_data_connection_timeout);
    }
    else if(tunable_idle_session_timeout > 0)
    {
        alarm(0);
    }
}

void check_abor(session_t *sess)
{
    if (sess->abor_received)
    {
        sess->abor_received = 0;
        ftp_reply(sess, FTP_ABOROK, "ABOR successful.");
    }
}

int get_port_fd(session_t *sess)
{
    priv_sock_send_cmd(sess->child_fd, PRIV_SOCK_GET_DATA_SOCK);
    unsigned short port = ntohs(sess->port_addr->sin_port);
    char *ip = inet_ntoa(sess->port_addr->sin_addr);

    priv_sock_send_int(sess->child_fd, (int)port);
    priv_sock_send_buf(sess->child_fd, ip, strlen(ip));
    char res = priv_sock_get_result(sess->child_fd);
    if (res == PRIV_SOCK_RESULT_BAD)
    {
        return 0;
    }
    else if (res == PRIV_SOCK_RESULT_OK)
    {
        sess->data_fd = priv_sock_recv_fd(sess->child_fd);
    }

    return 1;
}

int get_pasv_fd(session_t *sess)
{

    priv_sock_send_cmd(sess->child_fd, PRIV_SOCK_PASV_ACCEPT);
    char res = priv_sock_get_cmd(sess->child_fd);
    if (res == PRIV_SOCK_RESULT_BAD)
    {
        return 0;
    }
    else if (res == PRIV_SOCK_RESULT_OK)
    {
        sess->data_fd = priv_sock_recv_fd(sess->child_fd);
    }

    return 1;
    /*
    int fd = accept_timeout(sess->pasv_listen_fd, NULL, tunable_accept_timeout);
    //得到一个已连接套接字
    close(sess->pasv_listen_fd);
    sess->pasv_listen_fd = -1;
    sess->data_fd = fd;
    if (fd == -1)
    {
        return 0;
    }
    return 1;
     */
}
int get_transfer_fd(session_t *sess)
{
    // 检测是否收到post和pasv命令
    if (!port_active(sess) && !pasv_active(sess))
    {
        ftp_reply(sess, FTP_BADSENDCONN, "Use PORT or PASV first");
        return 0;
    }

    int ret = 1;
    // 主动模式
    if (port_active(sess))
    {
        /*
        //tcp_clint(20)
        int fd = tcp_client(0);
        if (connect_timeout(fd, sess->port_addr, tunable_connect_timeout) < 0 )
        {
            close(fd);
            return 0;
        }
        sess->data_fd = fd;
        */
        if (get_port_fd(sess) == 0)
        {
            ret = 0;
        }
    }

    if (pasv_active(sess))
    {
        /*
        int fd = accept_timeout(sess->pasv_listen_fd, NULL, tunable_accept_timeout);
        close(sess->pasv_listen_fd);
        if (fd == -1)
        {
            ret = 0;
        }

        sess->pasv_listen_fd = -1;
        sess->data_fd = fd;
         */
        if (get_pasv_fd(sess) == 0)
        {
            ret = 0;
        }
    }


    if (sess->port_addr)
    {
        free(sess->port_addr);
        sess->port_addr = NULL;
    }

    //重新安装信号　并启动
    if (ret)
    {
        start_data_alarm();
    }
    return ret;
}
int port_active(session_t *sess)
{
    if (sess->port_addr != NULL)
    {
        if (pasv_active(sess))
        {
            fprintf(stderr, "both port and pasv are active");
        }
        return 1;
    }
    else
    {
        return 0;
    }
}
int pasv_active(session_t *sess)
{

    /*

         if (sess->pasv_listen_fd != -1)
         {
             if (port_active(sess))
             {
                 fprintf(stderr, "both port and pasv are active");
             }
             return 1;
         }
         return 0;
    */
    priv_sock_send_cmd(sess->child_fd, PRIV_SOCK_PASV_ACTIVE);
    int active = priv_sock_get_int(sess->child_fd);
    if (active == 1)
    {
        if (port_active(sess))
        {
            fprintf(stderr, "both port and pasv are active");
            exit(EXIT_FAILURE);
        }
        return 1;
    }
    return 0;

}

/**
 * 返回０失败，返回１成功。
 */
int list_common(session_t *sess, int detail)
{
    DIR *dir = opendir(".");
    if (dir == NULL)
        return 0;

    struct dirent *dt;
    struct stat sbuf;
    while ((dt = readdir(dir)) != NULL)
    {
        if ((lstat(dt->d_name, &sbuf)) < 0)
        {
            continue;
        }

        if (dt->d_name[0] == '.')
        {
            continue;
        }
        char buf[1024] = {0};
        if (detail)
        {
            const char *power = statbuf_get_perms(&sbuf);
            int off = sprintf(buf, "%s", power);
            off += sprintf(buf + off, " %3d %-8d %-8d", (int)sbuf.st_nlink, (int)sbuf.st_uid, (int)sbuf.st_gid);
            off += sprintf(buf + off, "%8lu ", sbuf.st_size);

            const char *databuf = stabuf_get_date(&sbuf);
            off += sprintf(buf + off, "%s ", databuf);

            if (S_ISLNK(sbuf.st_mode))
            {
                char tmp[1024] = {0};
                readlink(dt->d_name, tmp, sizeof(tmp));
                off += sprintf(buf + off, "%s -> %s\r\n", dt->d_name, tmp);
            } else
            {
                off += sprintf(buf + off, "%s\r\n", dt->d_name);
            }
        }
        else
        {
            sprintf(buf, "%s\r\n", dt->d_name);
        }
        writen(sess->data_fd, buf, strlen(buf));
    }
    closedir(dir);
    return 1;
}

void limit_rate(session_t *sess, int bytes_transfered, int is_upload)
{
    //设定处于传输状态
    sess->data_process = 1;

    long curr_sec = get_time_sec();
    long curr_usec = get_time_usec();

    double elapsed;
    elapsed = curr_sec - sess->bw_transfer_start_sec;
    elapsed +=  (double)(curr_usec - sess->bw_transfer_start_usec) / (double)1000000;

    if (elapsed <= (double)0)
    {
        elapsed = 0.01;
    }
    //计算当前速度
    unsigned int bw_rate = (unsigned int)((double)bytes_transfered / elapsed);

    double rate_ratio;
    if (is_upload)
    {
        if(bw_rate <= sess->bw_upload_rate_max)
        {
            // 不需要限速
            sess->bw_transfer_start_sec = curr_sec;
            sess->bw_transfer_start_usec = curr_usec;
            return;
        }
        rate_ratio = bw_rate / sess->bw_upload_rate_max;
    }
    else
    {
        if(bw_rate < sess->bw_download_rate_max)
        {
            // 不需要限速
            sess->bw_transfer_start_sec = curr_sec;
            sess->bw_transfer_start_usec = curr_usec;
            return;
        }
        rate_ratio = bw_rate / sess->bw_download_rate_max;
    }

    double pavse_time;
    pavse_time = (rate_ratio - (double)1 ) * elapsed;
    nano_sleep(pavse_time);

    sess->bw_transfer_start_sec = get_time_sec();
    sess->bw_transfer_start_sec = get_time_usec();
}

static void upload_common(session_t *sess, int is_append)
{
    //创建数据链接
    if (get_transfer_fd(sess) == 0)
    {
        return;
    }

    long long offset = sess->restart_pos;
    sess->restart_pos = 0;

    //打开文件　
    int fd = open(sess->arg, O_CREAT | O_WRONLY, 0666);
    if (fd == -1)
    {
        ftp_reply(sess, FTP_UPLOADFAIL, "Could not creat file");
        return;
    }
    //加读锁
    int ret = lock_file_write(fd);
    if (ret == -1)
    {
        ftp_reply(sess,  FTP_UPLOADFAIL, "Could not creat file");
        return;
    }

    //STOR
    //REST + STOR
    //appe
    if (!is_append && offset == 0)
    {   // STOR
        ftruncate(fd, 0);
        if (lseek(fd, 0, SEEK_SET) < 0)
        {
            ftp_reply(sess, FTP_UPLOADFAIL, "Could not create file.");
            return;
        }
    }
    else if (!is_append && offset != 0)
    { // REST+STOR
        if (lseek(fd, offset, SEEK_SET) < 0)
        {
            ftp_reply(sess, FTP_UPLOADFAIL, "Could not create file.");
            return;
        }
    }
    else if (is_append)
    {   // APPE
        if (lseek(fd, 0, SEEK_END) < 0)
        {
            ftp_reply(sess, FTP_UPLOADFAIL, "Could not create file.");
            return;
        }
    }
    //150
    struct stat sbuf;
    fstat(fd, &sbuf);
    if (!S_ISREG(sbuf.st_mode))
    {
        ftp_reply(sess, FTP_UPLOADFAIL, "Could not create file.");
        return;
    }

    //150
    char text[1024] = {0};
    if (sess->is_ascii)
    {
        sprintf(text, "Opening ASCII mode data connection for %s (%lld bytes).",
                sess->arg, (long long)sbuf.st_size);
    }
    else
    {
        sprintf(text, "Opening BINARY mode data connection for %s (%lld bytes).",
                sess->arg, (long long)sbuf.st_size);
    }
    //ＡＳＣＩＩ转化　(选做）

    ftp_reply(sess, FTP_DATACONN, text);

    //下载文件
    int flag = 0;
/*
    long long bytes_to_send = sbuf.st_size;
    if (offset > bytes_to_send)
    {
        bytes_to_send = 0;
    }
    if (offset != 0)
    {
        bytes_to_send -= offset;
    }

    while (bytes_to_send)
    {
        long long num_this_time = bytes_to_send > 4096 ? 4096 : bytes_to_send;
        ret = sendfile(fd, sess->data_fd, NULL, 4096);
        if (ret == -1)
        {
            flag = 2;
            break;
        }
        bytes_to_send -= ret;
    }

    if (bytes_to_send == 0)
    {
        flag = 0;
    }*/
    sess->bw_transfer_start_sec = get_time_sec();
    sess->bw_transfer_start_usec = get_time_usec();
    char buf[1024];
    while (1)
    {
        ret = read(sess->data_fd, buf, sizeof(buf));
        if (ret == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                flag = 2;
                break;
            }
        }
        else if (ret == 0)
        {
            flag = 0;
            break;
        }
        limit_rate(sess, ret, 1);
        if (sess->abor_received)
        {
            flag = 2;
            break;
        }
        if (writen(fd, buf, ret) != ret)
        {
            flag = 1;
            break;
        }
    }
    //关闭数据套接字
    close(sess->data_fd);
    sess->data_fd = -1;

    close(fd);
    //226
    if (flag == 0 && !sess->abor_received)
    {
        ftp_reply(sess, FTP_TRANSFEROK, "Transfer complete.");
    }
    else if (flag == 1)
    {
        ftp_reply(sess, FTP_BADSENDFILE, "Failure writing from local file");
    }
    else if (flag == 2)
    {
        ftp_reply(sess, FTP_BADSENDNET, "Failure reading to network stream,");
    }

    check_abor(sess);
    start_cmdio_alarm();
}

void handle_child(session_t *sess)
{  
    writen(sess->ctrl_fd, (void *)"220 (minifpt 0.1)\r\n", strlen("220 (minifpt 0.1)\r\n"));
    int ret;
    while(1)
    {
        memset(sess->cmdline, 0, sizeof(sess->cmdline));
        memset(sess->cmd, 0, sizeof(sess->cmd));
        memset(sess->arg, 0, sizeof(sess->arg));

        start_cmdio_alarm();
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
        printf("cmd = %s, arg = %s\n", sess->cmd, sess->arg);
        int size = sizeof(ctrl_cmds) / sizeof(ctrl_cmds[0]);
        int i = 0;
        for (i = 0; i < size; i++)
        {
            if (strcmp(sess->cmd, ctrl_cmds[i].cmd) == 0)
            {
                if (ctrl_cmds[i].cmdhandler != NULL)
                {
                    ctrl_cmds[i].cmdhandler(sess);
                }
                else
                {
                    ftp_reply(sess, FTP_COMMANDNOTIMPL, "Unimplement command.");
                }
                break;
            }
        }
        if (i == size)
        {
            ftp_reply(sess, FTP_BADCMD, "Unknown command.");
        }
    }
    
}


void ftp_reply(session_t *sess, int status, const char *text)
{
    char buf[1024] = {0};
    sprintf(buf, "%d %s\r\n",status, text);
    writen(sess->ctrl_fd, buf, strlen(buf));
}

void ftp_lreply(session_t *sess, int status, const char *text)
{
    char buf[1024] = {0};
    sprintf(buf, "%d-%s\r\n",status, text);
    writen(sess->ctrl_fd, buf, strlen(buf));
}

static void do_user(session_t *sess)
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

    signal(SIGURG, handle_sigurg);
    activate_sigurg(sess->ctrl_fd);

    umask(tunable_local_umask);
    setegid(pw->pw_gid);
    seteuid(pw->pw_uid);
    chdir(pw->pw_dir);
    ftp_reply(sess, FTP_LOGINOK, "Login success.");

}

static void do_syst(session_t *sess)
{
    ftp_reply(sess, FTP_SYSTOK, "UNIX Type: L8");
}


static void do_feat(session_t *sess)
{
    ftp_lreply(sess, FTP_FEAT, "Features:");
    writen(sess->ctrl_fd, " EPRT\r\n", strlen(" EPRT\r\n"));
    writen(sess->ctrl_fd, " EPSV\r\n", strlen(" EPSV\r\n"));
    writen(sess->ctrl_fd, " MDTM\r\n", strlen(" MDTM\r\n"));
    writen(sess->ctrl_fd, " PASV\r\n", strlen(" PASV\r\n"));
    writen(sess->ctrl_fd, " REST STREAM\r\n", strlen(" REST STREAM\r\n"));
    writen(sess->ctrl_fd, " SIZE\r\n", strlen(" SIZE\r\n"));
    writen(sess->ctrl_fd, " TVFS\r\n", strlen(" TVFS\r\n"));
    writen(sess->ctrl_fd, " UTF8\r\n", strlen(" UTF8\r\n"));
    ftp_reply(sess, FTP_FEAT, "End");
}


static void do_cwd(session_t *sess)
{
    if (chdir(sess->arg) >= 0)
    {
        ftp_reply(sess, FTP_CWDOK, "Directory successfully changed.");
        return;
    }
    ftp_reply(sess, FTP_FILEFAIL, "Fail to change directory");
}


static void do_cdup(session_t *sess)
{
    if (chdir("..") >= 0)
    {
        ftp_reply(sess, FTP_CWDOK, "Directory successfully changed.");
        return;
    }
    ftp_reply(sess, FTP_FILEFAIL, "Fail to change directory");
}


static void do_quit(session_t *sess)
{
    ftp_reply(sess, FTP_GOODBYE, "Good bye");
    exit(EXIT_SUCCESS);
}


static void do_port(session_t *sess)
{
    //PORT 127,0,0,1,227,31
    unsigned int v[6];
    sscanf(sess->arg, "%u,%u,%u,%u,%u,%u",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5]);
    sess->port_addr = (struct sockaddr_in *)malloc(sizeof(sockaddr_in));
    sess->port_addr->sin_family = AF_INET;
    unsigned char * p = (unsigned char *)&sess->port_addr->sin_port;
    p[0] = v[4];

    p[1] = v[5];
    p = (unsigned char *)&sess->port_addr->sin_addr;
    p[0] = v[0];
    p[1] = v[1];
    p[2] = v[2];
    p[3] = v[3];
    ftp_reply(sess, FTP_PORTOK, "PORT command successful. Consider using PASV.");
}

static void do_pasv(session_t *sess)
{
    char ip[16] = {0};
    getlocalip(ip);
    /*
    sess->pasv_listen_fd = tcp_server(ip, 0);
    struct sockaddr_in addr;
    socklen_t socklen = sizeof(addr);
    if (getsockname(sess->pasv_listen_fd, (struct sockaddr *)&addr, &socklen) < 0)
    {
        ERR_EXIT("getsockname");
    }

    unsigned short port = ntohs(addr.sin_port);
    */
    priv_sock_send_cmd(sess->child_fd, PRIV_SOCK_PASV_LISTEN);
    unsigned short port = (int)priv_sock_get_int(sess->child_fd);


    unsigned int v[4];
    sscanf(ip, "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]);
    char text[1024] = {0};
    sprintf(text, "Entering Passive Mode (%u,%u,%u,%u,%u,%u).",
            v[0], v[1], v[2], v[3], port>>8, port&0xFF);

    ftp_reply(sess, FTP_PASVOK, text);

}


static void do_type(session_t *sess)
{
    if (strcmp(sess->arg, "A") == 0)
    {
        ftp_reply(sess, FTP_TYPEOK, "Switching to ASCII mode.");
    }
    else if (strcmp(sess->arg, "I") == 0)
    {
        ftp_reply(sess, FTP_TYPEOK, "Switching to Binary mode.");
    }
    else
    {
        ftp_reply(sess, FTP_BADCMD, "Unrecognised Type cmd.");
    }
}


static void do_retr(session_t *sess)
{
    //下载文件
    //断点续传

    //创建数据链接
    if (get_transfer_fd(sess) == 0)
    {
        return;
    }

    long long offset = sess->restart_pos;
    sess->restart_pos = 0;

    //打开文件　
    int fd = open(sess->arg, O_RDONLY);
    if (fd == -1)
    {
        ftp_reply(sess, FTP_FILEFAIL, "Failed to open file.");
        return;
    }
    //加读锁
    int ret = lock_file_read(fd);
    if (ret < 0)
    {
        ftp_reply(sess, FTP_FILEFAIL, "Failed to open file.");
        return;
    }

    //判断是否是一个普通文件
    struct stat sbuf;
    ret = fstat(fd, &sbuf);
    if (ret < 0 || !S_ISREG(sbuf.st_mode))
    {
        ftp_reply(sess, FTP_FILEFAIL, "Failed to open file.");
        return;
    }

    if (offset != 0)
    {
        ret = lseek(fd, offset, SEEK_SET);
        if (ret == -1)
        {
            ftp_reply(sess, FTP_FILEFAIL, "Failed to open file.");
            return;
        }
    }
    //150
    char text[1024] = {0};
    if (sess->is_ascii)
    {
        sprintf(text, "Opening ASCII mode data connection for %s (%lld bytes).",
                sess->arg, (long long)sbuf.st_size);
    }
    else
    {
        sprintf(text, "Opening BINARY mode data connection for %s (%lld bytes).",
                sess->arg, (long long)sbuf.st_size);
    }
    //ＡＳＣＩＩ转化　(选做）

    ftp_reply(sess, FTP_DATACONN, text);

    //下载文件
    int flag = 0;
    /*
    char buf[4096];
    while (1)
    {
        ret = read(fd, buf, sizeof(buf));
        if (ret == -1)
        {
            if (errno = EINTR)
            {
                continue;
            }
            else
            {
                flag = 1;
                break;
            }
        }
        else if (ret == 0)
        {
            flag = 0;
            break;
        }

        if (writen(sess->data_fd, buf, ret) != ret)
        {
            flag = 2;
            break;
        }
    }
*/
    long long bytes_to_send = sbuf.st_size;
    if (offset > bytes_to_send)
    {
        bytes_to_send = 0;
    }
    if (offset != 0)
    {
        bytes_to_send -= offset;
    }

    while (bytes_to_send)
    {
        long long num_this_time = bytes_to_send > 4096 ? 4096 : bytes_to_send;
        ret = sendfile(sess->data_fd, fd, NULL, num_this_time);
        if (ret == -1)
        {
            flag = 2;
            break;
        }
        limit_rate(sess, ret, 0);
        if (sess->abor_received == 1)
        {
            flag = 2;
            break;
        }
        bytes_to_send -= ret;
    }

    if (bytes_to_send == 0)
    {
        flag = 0;
    }
    //关闭数据套接字
    close(sess->data_fd);
    sess->data_fd = -1;
    close(fd);
    fd = -1;
    //226
    if (flag == 0 && !sess->abor_received)
    {
        ftp_reply(sess, FTP_TRANSFEROK, "Transfer complete.");
    }
    else if (flag == 1)
    {
        ftp_reply(sess, FTP_BADSENDFILE, "Failure reading from local file");
    }
    else if (flag == 2)
    {
        ftp_reply(sess, FTP_BADSENDNET, "Failure writting to network stream,");
    }

    check_abor(sess);
    // 重新开启控制连接闹钟
    start_cmdio_alarm();
}

static void do_stor(session_t *sess)
{
    upload_common(sess, 0);
}

static void do_appe(session_t *sess)
{
    upload_common(sess, 1);
}

static void do_list(session_t *sess)
{
    //创建数据链接
    if (get_transfer_fd(sess) == 0)
    {
        return;
    }
    //150
    ftp_reply(sess, FTP_DATACONN, "Here comes the directory listing.");
    //传输列表
    list_common(sess, 1);
    //关闭数据套接字
    close(sess->data_fd);
    sess->data_fd = -1;
    //226
    ftp_reply(sess, FTP_TRANSFEROK,"Directory send OK.");
}


static void do_nlst(session_t *sess)
{
    //创建数据链接
    if (get_transfer_fd(sess) == 0)
    {
        return;
    }
    //150
    ftp_reply(sess, FTP_DATACONN, "Here comes the directory listing.");
    //传输列表
    list_common(sess, 0);
    //关闭数据套接字
    close(sess->data_fd);
    sess->data_fd = -1;
    //226
    ftp_reply(sess, FTP_TRANSFEROK,"Directory send OK.");
}

static void do_rest(session_t *sess)
{
    //atoll;
    sess->restart_pos = str_to_longlong(sess->arg);
    char text[1024] = {0};
    sprintf(text, "Restart position accpted (%lld)", sess->restart_pos);
    ftp_reply(sess, FTP_RESTOK, text);
}

static void do_abor(session_t *sess)
{
    ftp_reply(sess, FTP_ABOR_NOCONN, "NO transfer to ABOR.");
}

static void do_pwd(session_t *sess)
{
    char dir[1024] = {0};
    char buf[1024] = {0};
    list_common(sess, 0);
    getcwd(dir, 1024);
    sprintf(buf, "\"%s\"", dir);
    ftp_reply(sess, FTP_PWDOK, buf);
}

static void do_mkd(session_t *sess)
{
    if (mkdir(sess->arg, 0777) < 0)
    {
        ftp_reply(sess, FTP_FILEFAIL, "Create directory failed.");
        return;
    }
    char text[4096] = {0};
    if (sess->arg[0] == '/')
    {
        sprintf(text,"%s create", sess->arg);
    }
    else
    {
        char dir[4096+1] = {0};
        getcwd(dir, sizeof(dir));
        if (dir[strlen(dir) - 1] == '/')
        {
            sprintf(text,"%s%s create",dir, sess->arg);
        }
        else
        {
            sprintf(text,"%s/%s create",dir, sess->arg);
        }
    }
    ftp_reply(sess, FTP_MKDIROK, text);
}

static void do_rmd(session_t *sess)
{
    if (rmdir(sess->arg) < 0)
    {
        ftp_reply(sess, FTP_FILEFAIL, "Remove Directory failed.");
        return;
    }
    ftp_reply(sess, FTP_RMDIROK, "Remove Directory Successful.");
}

static void do_dele(session_t *sess)
{
    if (unlink(sess->arg) < 0)
    {
        ftp_reply(sess, FTP_FILEFAIL, "Delete file failed.");
        return;
    }
    ftp_reply(sess, FTP_DELEOK, "Delete file Successful.");
}

static void do_rnfr(session_t *sess)
{
    sess->rnfr_name = (char *)malloc(strlen(sess->arg) + 1);
    memset(sess->rnfr_name, 0, strlen(sess->arg) + 1);
    strcpy(sess->rnfr_name, sess->arg);
    ftp_reply(sess, FTP_RNFROK, "Ready for rnto.");
}

static void do_rnto(session_t *sess)
{
    if (sess->rnfr_name == NULL)
    {
        ftp_reply(sess, FTP_NEEDRNFR, "RNFP required frist.");
        return;
    }
    if (rename(sess->rnfr_name, sess->arg) < 0)
    {
        ftp_reply(sess, FTP_FILEFAIL, "Rename failed.");
    }
    ftp_reply(sess, FTP_RENAMEOK, "Rename successful.");
    free(sess->rnfr_name);
    sess->rnfr_name = NULL;
}

static void do_site(session_t *sess)
{
    char cmd[100] = {0};
    char arg[100] = {0};

    str_split(sess->arg, cmd, arg, ' ');
    if (strcmp(cmd, "CHMOD") == 0)
    {
        do_site_chmod(sess, arg);
    }
    else if (strcmp(cmd, "UMASK") == 0)
    {
        do_site_umask(sess, arg);
    }
    else if (strcmp(cmd, "HELP") == 0)
    {
        ftp_reply(sess, FTP_SITEHELP, "CHMOD UMASK HELP");
    }
    else
    {
        ftp_reply(sess, FTP_BADCMD, "Unknown SITE command.");
    }
}
static void do_size(session_t *sess)
{
    struct stat buf;
    if (stat(sess->arg, &buf) < 0)
    {
        ftp_reply(sess, FTP_FILEFAIL, "Size failed.");
        return;
    }

    if (!S_ISREG(buf.st_mode))
    {
        ftp_reply(sess, FTP_FILEFAIL, "Could not get file size.");
        return;
    }

    char text[1024] = {0};
    sprintf(text, "%ld", buf.st_size);
    ftp_reply(sess, FTP_SIZEOK, "Could not get file size.");
}

static void do_stat(session_t *sess)
{
    ftp_lreply(sess, FTP_STATOK, "FTP server status:");
    if (sess->bw_upload_rate_max == 0) {
        char text[1024];
        sprintf(text,
                "     No session upload bandwidth limit\r\n");
        writen(sess->ctrl_fd, text, strlen(text));
    }
    else if (sess->bw_upload_rate_max > 0) {
        char text[1024];
        sprintf(text,
                "     Session upload bandwidth limit in byte/s is %u\r\n",
                sess->bw_upload_rate_max);
        writen(sess->ctrl_fd, text, strlen(text));
    }

    if (sess->bw_download_rate_max == 0) {
        char text[1024];
        sprintf(text,
                "     No session download bandwidth limit\r\n");
        writen(sess->ctrl_fd, text, strlen(text));
    }
    else if (sess->bw_download_rate_max > 0) {
        char text[1024];
        sprintf(text,
                "     Session download bandwidth limit in byte/s is %u\r\n",
                sess->bw_download_rate_max);
        writen(sess->ctrl_fd, text, strlen(text));
    }

    char text[1024] = {0};
    sprintf(text,
            "     At session startup, client count was %u\r\n",
            sess->num_clients);
    writen(sess->ctrl_fd, text, strlen(text));

    ftp_reply(sess, FTP_STATOK, "End of status");
}

static void do_noop(session_t *sess)
{
    ftp_reply(sess, FTP_NOOPOK, "NOOP ok");
}

static void do_help(session_t *sess)
{
    ftp_lreply(sess, FTP_HELP, "The following commands are recognized");
    writen(sess->ctrl_fd, "ABOR ACCT ALLO APPE CDUP CWD  DELE EPRT EPSV FEAT HELP LIST MDTM MKD\r\n", strlen("ABOR ACCT ALLO APPE CDUP CWD  DELE EPRT EPSV FEAT HELP LIST MDTM MKD\r\n"));
    writen(sess->ctrl_fd, "MODE NLST NOOP OPTS PASS PASV PORT PWD  QUIT REIN REST RETR RMD  RNFR\r\n", strlen("MODE NLST NOOP OPTS PASS PASV PORT PWD  QUIT REIN REST RETR RMD  RNFR\r\n"));
    writen(sess->ctrl_fd, "RNTO SITE SIZE SMNT STAT STOR STOU STRU SYST TYPE USER XCUP XCWD XMKD\r\n", strlen("RNTO SITE SIZE SMNT STAT STOR STOU STRU SYST TYPE USER XCUP XCWD XMKD\r\n"));
    writen(sess->ctrl_fd, "XPWD XRMD\r\n", strlen("XPWD XRMD\r\n"));
    ftp_reply(sess, FTP_HELP,"HELP OK");
}


static void do_site_chmod(session_t *sess, char *chmod_arg)
{
    if (strlen(chmod_arg) == 0)
    {
        ftp_reply(sess, FTP_BADCMD, "SITE CHMOD needs 2 arguments.");
        return;
    }
    char perm[100] = {0};
    char file[100] = {0};
    str_split(chmod_arg , perm, file, ' ');
    if (strlen(file) == 0)
    {
        ftp_reply(sess, FTP_BADCMD, "SITE CHMOD needs 2 arguments.");
        return;
    }

    unsigned int mode = str_octal_to_uint(perm);
    if (chmod(file, mode) < 0)
    {
        ftp_reply(sess, FTP_CHMODOK, "SITE CHMOD command failed.");
    }
    else
    {
        ftp_reply(sess, FTP_CHMODOK, "SITE CHMOD command ok.");
    }
}
static void do_site_umask(session_t *sess, char *umask_arg)
{
    if (strlen(umask_arg) == 0)
    {
        char text[1024] = {0};
        sprintf(text, "Your current UMASK is 0%o", tunable_local_umask);
        ftp_reply(sess, FTP_UMASKOK, text);
    }
    else
    {
        unsigned int um = str_octal_to_uint(umask_arg);
        umask(um);
        char text[1024] = {0};
        sprintf(text, "UMASK set to 0%o", um);
        ftp_reply(sess, FTP_UMASKOK, text);
    }
}
