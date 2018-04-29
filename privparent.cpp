#include "privparent.h"
#include "privsock.h"


static void privop_pasv_get_data_sock(session_t *sess);
static void privop_pasv_active(session_t *sess);
static void privop_pasv_listen(session_t *sess);
static void privop_pasv_accept(session_t *sess);


void handle_parent(session_t* sess) {
    struct passwd *pw = getpwnam("nobody");
    if (pw == NULL)
        return;
    if (setegid(pw->pw_gid) < 0) {
        ERR_EXIT("setegid");
    }
    if (seteuid(pw->pw_uid) < 0) {
        ERR_EXIT("seteuid");
    }

    char cmd;
    while (1)
    {
        cmd = priv_sock_get_cmd(sess->parent_fd);
        //解析内部命令 处理内部命令
        switch (cmd) {
            case PRIV_SOCK_GET_DATA_SOCK:
                break;
            case PRIV_SOCK_PASV_ACTIVE:
                break;
            case PRIV_SOCK_PASV_LISTEN:
                break;
            case PRIV_SOCK_PASV_ACCEPT:
                break;
        }
    }
}
static void privop_pasv_get_data_sock(session_t *sess)
{

}
static void privop_pasv_active(session_t *sess)
{

}
static void privop_pasv_listen(session_t *sess)
{

}
static void privop_pasv_accept(session_t *sess)
{

}