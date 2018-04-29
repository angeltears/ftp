#include "session.h"
#include "privparent.h"
#include "ftpproto.h"
#include "privsock.h"


void begin_session(session_t *sess)
{
    priv_sock_init(sess);
	pid_t pid;
	pid = fork();
	if (pid < 0)
		ERR_EXIT("fork");

	if (pid == 0) 
    {
		// ftp服务进程
		//close(sockfds[0]);
		//sess->child_fd = sockfds[1];
        priv_sock_set_child_context(sess);
		handle_child(sess);
	} 
    else 
    {
		// nobody进程
		//close(sockfds[1]);
		//sess->parent_fd = sockfds[0];
        priv_sock_set_parent_context(sess);
		handle_parent(sess);
	}
}