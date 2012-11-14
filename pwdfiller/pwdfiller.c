#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


int print_help()
{
	printf("Usage: pwdfiller -c <shell command>\n");

	return 0;
}


int pexec(char *cmd, char *pwd)
{
	int rc = 0;
	FILE *fin = NULL;

    setpgid(0, getppid());
	setsid();					//drop the controlling tty

	fin = popen(cmd, "w");
	if(fin==NULL) 
	{
		rc = -1;
		goto shutdown;
	}

	fputs(pwd, fin);
	rc = pclose(fin);
	rc = rc<0 ? rc : WEXITSTATUS(rc);
	
shutdown:
	return rc;
}



int main(int argc, char * argv[])
{
	int rc = 0;
	char pwd[128];
	char cmd[10240]="exec";

	if(argc<3)
	{
		print_help();
		exit(1);
	}
	
	int i = 0;
	for(i=2; i<argc; i++)
	{
		sprintf(cmd, "%s '%s'", cmd, argv[i]);	//there cannot be "'" in the arguments
	}
	//printf("%s\n", cmd);
	
	fgets(pwd, sizeof(pwd), stdin);
	int len = strlen(pwd);
	if(pwd[len-1]=='\n') pwd[len-1] = '\0';

	rc = pexec(cmd, pwd);	
	if(rc == -1)
	{
		printf("E: failed to execute the command.");
		exit(255);
	}

	return rc;
}


