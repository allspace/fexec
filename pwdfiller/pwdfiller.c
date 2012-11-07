#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


int print_help()
{
	printf("Usage: pwdfiller <shell command>");

	return 0;
}


int pexec(char *cmd, char *pwd)
{
	int rc = 0;
	FILE *fin = NULL;

        setpgid(0, getppid());
	setsid();

	fin = popen(cmd, "w");
	if(fin==NULL) 
	{
		rc = -1;
		goto shutdown;
	}

	fputs(pwd, fin);
	pclose(fin);

shutdown:
	return rc;
}



int main(int argc, char * argv[])
{
	int rc = 0;
	char pwd[128];

	if(argc<2)
	{
		print_help();
		exit(1);
	}	
	
	fgets(pwd, sizeof(pwd), stdin);
	int len = strlen(pwd);
	if(pwd[len-1]=='\n') pwd[len-1] = '\0';

	rc = pexec(argv[1], pwd);	
	if(rc != 0)
	{
		printf("E: failed to execute the command.");
		exit(1);
	}

	return 0;
}


