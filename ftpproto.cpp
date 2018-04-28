#include "ftpproto.h"
#include "sysutil.h"
#include "str.h"
#include "ftpcode.h"
#include "tunable.h"

void ftp_lreply(session_t *sess, int status, const char *text);

void handle_alarm_timeout(int sig);
void handle_sigalrm(int sig);
void handle_sigurg(int sig);
void start_cmdio_alarm(void);
void start_data_alarm(void);

void check_abor(session_t *sess);

int list_common(session_t *sess, int detail);
void limit_rate(session_t *sess, int bytes_transfered, int is_upload);
void upload_common(session_t *sess, int is_append);

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


int get_port_fd(session_t *sess);
int get_pasv_fd(session_t *sess);
int get_transfer_fd(session_t *sess)
{
    // 检测是否收到post和pasv命令
    if (!port_active(sess) && !pasv_active(sess))
    {
        return 0;
    }
    // 主动模式
    if (port_active(sess))
    {
        //tcp_clint(20)
        int fd = tcp_client(0);
        if (connect_timeout(fd, sess->port_addr, tunable_connect_timeout) < 0 )
        {
            close(fd);
            return 0;
        }
        sess->data_fd = fd;
    }
    if (sess->port_addr)
    {
        free(sess->port_addr);
        sess->port_addr = NULL;
    }
    return 1;
}
int port_active(session_t *sess)
{
    if (sess->port_addr != NULL)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
int pasv_active(session_t *sess)
{
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
        char power[] = "----------";
        power[0] = '?';

        mode_t mode = sbuf.st_mode;
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

        char buf[1024] = {0};
        int off = sprintf(buf, "%s", power);
        off += sprintf(buf + off, " %3d %-8d %-8d", sbuf.st_nlink, sbuf.st_uid, sbuf.st_gid);
        off += sprintf(buf + off, "%8lu ", sbuf.st_size);

        const char *p_data_format = "%b %e %H:%M";
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t local_time = tv.tv_sec;
        if(sbuf.st_mtime > local_time || (local_time - sbuf.st_mtime) > 182 * 24 * 60 *60);
        {
            p_data_format = "%b %e %Y";
        }

        char databuf[64] = {0};
        struct tm * p_tm = localtime(&local_time);
        strftime(databuf, sizeof(databuf), p_data_format, p_tm);
        off += sprintf(buf + off, "%s ", databuf);
        if (S_ISLNK(sbuf.st_mode))
        {
            char tmp[1024] = {0};
            readlink(dt->d_name, tmp, sizeof(tmp));
            off +=  sprintf(buf + off, "%s -> %s\r\n",dt->d_name, tmp);
        }
        else
        {
            off += sprintf(buf + off, "%s\r\n",dt->d_name);
        }
        writen(sess->data_fd, buf, strlen(buf));
    }
    closedir(dir);
    return 1;
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
        if (i == size) {
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

}
static void do_cdup(session_t *sess)
{

}
static void do_quit(session_t *sess)
{

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

}
static void do_stor(session_t *sess)
{

}
static void do_appe(session_t *sess)
{

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
    list_common(sess, 0);
    //关闭数据套接字
    close(sess->data_fd);
    //226
    ftp_reply(sess, FTP_TRANSFEROK,"Directory send OK.");
}
static void do_nlst(session_t *sess)
{

}
static void do_rest(session_t *sess)
{

}
static void do_abor(session_t *sess)
{

}
static void do_pwd(session_t *sess)
{
    char dir[1024] = {0};
    char buf[1024] = {0};
    list_common(NULL, 0);
    getcwd(dir, 1024);
    sprintf(buf, "\"%s\"", dir);
    ftp_reply(sess, FTP_PWDOK, buf);
}
static void do_mkd(session_t *sess)
{

}
static void do_rmd(session_t *sess)
{

}
static void do_dele(session_t *sess)
{

}
static void do_rnfr(session_t *sess)
{

}
static void do_rnto(session_t *sess)
{

}
static void do_site(session_t *sess)
{

}
static void do_size(session_t *sess)
{

}
static void do_stat(session_t *sess)
{

}
static void do_noop(session_t *sess)
{

}
static void do_help(session_t *sess)
{

}