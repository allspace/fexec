#ifndef _FEXEC_H
#define _FEXEC_H

#include <libssh2.h>

typedef struct _SSH_CONNECTION
{
	int sock;
	LIBSSH2_SESSION *session;
}ssh_conn_t;

typedef struct _PARAM
{
	char hostname[128];
	int  port;
	char username[128];
	char password[128];
	char command[32*1024];
	char logfile[1024];
	
	int argc;
	char **argv;
}param_t;


int ssh_setenv(LIBSSH2_CHANNEL *channel, param_t *params);
ssh_conn_t *ssh_connect(param_t *params);
int ssh_auth(ssh_conn_t *conn, param_t *params);
int ssh_close(ssh_conn_t *pcon);
int ssh_exec(ssh_conn_t *conn, param_t *params);

#endif //_FEXEC_H
