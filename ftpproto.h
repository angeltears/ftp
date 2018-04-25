//
// Created by hyj on 18-4-25.
//
#include "session.h"
#ifndef FTP_FTPROTO_H
#define FTP_FTPROTO_H

void handle_child(session_t *sess);
void ftp_reply(session_t *sess, int status, const char *text);
#endif //FTP_FTPROTO_H
