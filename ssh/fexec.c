/*
 * Sample showing how to use libssh2 to execute a command remotely.
 *
 * The sample code has fixed values for host name, user name, password
 * and command to run.
 *
 * Run it like this:
 *
 * $ ./ssh2_exec 127.0.0.1 user password "uptime"
 *
 */



#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "rexec.h"

param_t g_param_list;



int print_help()
{
	printf("Usage:\n");
	printf("psrun --host=<hostname/IP> --user=<username> --password=<password> --command=<command line>\n");
	
	return 0;
}

int parse_input(int argc, char *argv[], param_t *params)
{
	int read_pass_stdin=0, read_cmd_stdin=0;
	int rc = 0;
	
	int i = 0;
	for(i=1; i<argc; i++)
	{
		char *param = argv[i];
		int len = strlen(param);
		
		if(len<3)
		{
			print_help();		
			exit(1);
		}
		
		if(param[0]!='-' || param[1]!='-') continue;
		
		if(strncmp("help", &param[2], 4)==0)
		{
			print_help();		
			exit(0);
		}
		
		int j = 0;
		int found = 0;
		for(j=2; j<len; j++)
		{			
			if(param[j]=='=')
			{
				//printf("&param[j+1]=%s\n", &param[j+1]);
				if(strncmp("host", &param[2], 4)==0)
				{				
					strncpy(params->hostname, &param[j+1], sizeof(params->hostname)-1);
				}else if(strncmp("user", &param[2], 4)==0){
					strncpy(params->username, &param[j+1], sizeof(params->username)-1);
				}else if(strncmp("password", &param[2], 8)==0){
					strncpy(params->password, &param[j+1], sizeof(params->password)-1);
				}else if(strncmp("command", &param[2], 7)==0){
					strncpy(params->command, &param[j+1], sizeof(params->command)-1);
				}else if(strncmp("script-file", &param[2], 11)==0){
					strncpy(params->command, &param[j+1], sizeof(params->command)-1);
					FILE *fp = fopen(params->command, "r");
					if(fp!=NULL)
					{
						rc = fread(params->command, sizeof(char), sizeof(params->command), fp);
						rc = ferror(fp);
						fclose(fp);
						
						if(rc!=0)
						{
							printf("Error: failed to read %s(%s).\n", &param[j+1], strerror(rc));
							exit(1);
						}
					}
				}
				found = 1;
				break;
			}
		}
		
		if(found) continue;
		
		if(strncmp("password", &param[2], 8)==0)
		{
			read_pass_stdin = 1;
		}else if(strncmp("command", &param[2], 7)==0){
			read_cmd_stdin = 1;
		}
		
	}
	
	
	if(read_pass_stdin)
	{
		fgets(params->password, sizeof(params->password)-1, stdin);
		int len = strlen(params->password);
		if(params->password[len-1]=='\n')params->password[len-1]='\0';
		printf("params->password=%s\n",params->password);
	}
	
	if(read_cmd_stdin)
	{		
		rc = fread(params->command, sizeof(char), sizeof(params->command), stdin);
	}
	
	if(strlen(params->username)==0)
	{
		char *user = getenv("USER");
		if(user)
		{
			strncpy(params->username, user, sizeof(params->username)-1);
		}
	}
	
	if(strlen(params->hostname)==0 
		|| strlen(params->username)==0
		|| strlen(params->command)==0)
	{
		print_help();		
		exit(2);
	}
	
	params->argc = argc;
	params->argv = argv;
	
	return 0;
}




int main(int argc, char *argv[])
{    
 
    int rc = 0;
	param_t *params = &g_param_list;

#ifdef WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,0), &wsadata);
#endif
	
	parse_input(argc, argv, params);

    ssh_conn_t *conn = ssh_connect(params);
	if(conn==NULL)
	{
		rc = 1;
		goto shutdown;
	}
	
	rc = ssh_auth(conn, params);
	if(rc!=0)
	{
		goto shutdown;
	}

	while(1)
	{
		rc = ssh_exec(conn, params);
		
		rc = ssh_keepalive(conn);
		if(rc<0) break;
		
		sleep(rc);
	}
	
shutdown:
	ssh_close(conn);	
	return rc;
}

