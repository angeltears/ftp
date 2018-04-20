#include "session.h"
#include "privparent.h"
#include "ftpproto.h"
#include "common.h"

void begin_session(session_t *sess)
{
	int sockfds[2];
	if((socketpair(PF_UNIX, SOCK_STREAM, 0,sockfds))< 0)
	{
		ERR_EXIT("socketpair");
	}

	pid_t pid;
	pid = fork();
	if (pid < 0)
		ERR_EXIT("fork");

	if (pid == 0) 
    {
		// ftp服务进程
		close(sockfds[0]);
		sess->child_fd = sockfds[1];
		handle_child(sess);
	} 
    else 
    {
		// nobody进程
		close(sockfds[1]);
		sess->parent_fd = sockfds[0];
		handle_parent(sess);
	}
}