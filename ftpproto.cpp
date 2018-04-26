#include "ftpproto.h"
#include "sysutil.h"
#include "str.h"
#include "ftpcode.h"

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
        printf("cmd = %s\n", sess->cmdline);
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