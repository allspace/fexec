
#include "libssh2_config.h"
#include <libssh2.h>

#ifdef HAVE_WINSOCK2_H
# include <winsock2.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
# ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include "rexec.h"

static int waitsocket(ssh_conn_t *conn)
{
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    FD_ZERO(&fd);

    FD_SET(conn->sock, &fd);

    /* now make sure we wait in the correct direction */
    dir = libssh2_session_block_directions(conn->session);

    if(dir & LIBSSH2_SESSION_BLOCK_INBOUND)
        readfd = &fd;

    if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
        writefd = &fd;

    rc = select(conn->sock + 1, readfd, writefd, NULL, &timeout);

    return rc;
}

int ssh_setenv(LIBSSH2_CHANNEL *channel, param_t *params)
{
	int i = 0, rc = 0;
	char varname[128];
	
	for(i=1; i<params->argc; i++)
	{
		char *param = params->argv[i];
		int len = strlen(param);
		
		if(len<3)
		{
			print_help();		
			exit(1);
		}
		
		if(param[0]=='-' || param[1]=='-') continue;
		
		int j = 0;
		for(j=2; j<len; j++)
		{			
			if(param[j]=='=')
			{
				if(j>sizeof(varname)-1) continue;
				strncpy(varname, param, j);
				varname[j] = '\0';
				do{
					rc = libssh2_channel_setenv(channel, varname, &param[j+1]);
					if(rc<0 && rc!=LIBSSH2_ERROR_EAGAIN) { return rc;}										
				}while(rc==LIBSSH2_ERROR_EAGAIN);
				break;
			}
		}
	}
	
	return 0;
}


ssh_conn_t *ssh_connect(param_t *params)
{
	int rc;
    struct sockaddr_in sin;
	unsigned long hostaddr;
	ssh_conn_t *pcon = (ssh_conn_t*)malloc(sizeof(ssh_conn_t));
	
    rc = libssh2_init (0);
    if (rc != 0) {
        fprintf (stderr, "libssh2 initialization failed (%d)\n", rc);
        return NULL;
    }
	
    /* Ultra basic "connect to port 22 on localhost"
     * Your code is responsible for creating the socket establishing the
     * connection
     */
	hostaddr = inet_addr(params->hostname);
    pcon->sock = socket(AF_INET, SOCK_STREAM, 0);

    sin.sin_family = AF_INET;
    sin.sin_port = htons(22);
    sin.sin_addr.s_addr = hostaddr;
    if (connect(pcon->sock, (struct sockaddr*)(&sin),
                sizeof(struct sockaddr_in)) != 0) {
        fprintf(stderr, "failed to connect!\n");
        return NULL;
    }

    /* Create a session instance */
    pcon->session = libssh2_session_init();
    if (!pcon->session)
        return NULL;

    /* tell libssh2 we want it all done non-blocking */
    libssh2_session_set_blocking(pcon->session, 0);

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    while ((rc = libssh2_session_handshake(pcon->session, pcon->sock)) ==
           LIBSSH2_ERROR_EAGAIN);
    if (rc) {
        fprintf(stderr, "Failure establishing SSH session: %d\n", rc);
        return NULL;
    }

	return pcon;
shutdown:
	return NULL;
}	

int ssh_auth(ssh_conn_t *conn, param_t *params)
{
	int rc = 0;
	
	//authentication start here
    if ( strlen(params->password) != 0 ) {
        /* We could authenticate via password */
        while ((rc = libssh2_userauth_password(conn->session, params->username, params->password)) ==
               LIBSSH2_ERROR_EAGAIN);
        if (rc) {
            fprintf(stderr, "Authentication by password failed.\n");
            goto shutdown;
        }
    }
    else {
        /* Or by public key */
        while ((rc = libssh2_userauth_publickey_fromfile(conn->session, params->username,
                                                         "/home/user/"
                                                         ".ssh/id_rsa.pub",
                                                         "/home/user/"
                                                         ".ssh/id_rsa",
                                                         params->password)) ==
               LIBSSH2_ERROR_EAGAIN);
        if (rc) {
            fprintf(stderr, "\tAuthentication by public key failed\n");
            goto shutdown;
        }
    }
	
	return 0;
	
shutdown:

	return -1;
}

int ssh_close(ssh_conn_t *pcon)
{
	if(pcon==NULL) return 0;
	
    libssh2_session_disconnect(pcon->session,
                               "Normal Shutdown, Thank you for playing");
    libssh2_session_free(pcon->session);

#ifdef WIN32
    closesocket(pcon->sock);
#else
    close(pcon->sock);
#endif

    libssh2_exit();
	
	free(pcon);
	
	return 0;
}




int ssh_exec(ssh_conn_t *conn, param_t *params)
{
	LIBSSH2_CHANNEL *channel;
    int rc;
    int exitcode = 127;
    char *exitsignal=(char *)"none";	
	const char *commandline = params->command;

    /* Exec non-blocking on the remove host */
    while( (channel = libssh2_channel_open_session(conn->session)) == NULL &&
           libssh2_session_last_error(conn->session,NULL,NULL,0) ==
           LIBSSH2_ERROR_EAGAIN )
    {
        waitsocket(conn);
    }
    if( channel == NULL )
    {
        fprintf(stderr,"Error\n");
        return ( 1 );
    }
	
	rc = ssh_setenv(channel, params);
	if(rc<0)
	{
		if(rc==LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED)
			printf("Error: Environment variable is not accepted by target SSH server.");
		goto shutdown;
	}
	
    while( (rc = libssh2_channel_exec(channel, commandline)) ==
           LIBSSH2_ERROR_EAGAIN )
    {
        waitsocket(conn);
    }
    if( rc != 0 )
    {
        fprintf(stderr,"Error\n");
        return ( 1 );
    }
	
	int stream_id = 0;
    for( ;; )
    {
        /* loop until we block */
        int rc;
        do
        {
            char buffer[0x4000];
            rc = libssh2_channel_read_ex( channel, stream_id, buffer, sizeof(buffer) );
            if( rc > 0 )
            {
				fwrite(buffer, sizeof(char), rc, stream_id==0?stdout:stderr);
            }else{
                if( rc != LIBSSH2_ERROR_EAGAIN ) goto shutdown;
            }
        }
        while( rc > 0 );

        /* this is due to blocking that would occur otherwise so we loop on
           this condition */
        if( rc == LIBSSH2_ERROR_EAGAIN )
        {
            waitsocket(conn);
        }
        else
            break;
			
		stream_id = stream_id==0?1:0;
    }
    
    while( (rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN )
        waitsocket(conn);

    if( rc == 0 )
    {
        exitcode = libssh2_channel_get_exit_status( channel );
        libssh2_channel_get_exit_signal(channel, &exitsignal,
                                        NULL, NULL, NULL, NULL, NULL);
    }


shutdown:
    libssh2_channel_free(channel);
    channel = NULL;

    return exitcode;
}

int ssh_keepalive(ssh_conn_t *conn)
{
	int rc = 0;
	int sec = 0;
	rc = libssh2_keepalive_send(conn->session, &sec);
	sec = sec-1<0?0:sec-1;
	return rc<0 ? rc : sec;
}
