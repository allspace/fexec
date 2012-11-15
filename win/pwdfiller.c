
#define _XOPEN_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/select.h>


#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#if HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


enum program_return_codes {
    RETURN_NOERROR,
    RETURN_INVALID_ARGUMENTS,
    RETURN_CONFLICTING_ARGUMENTS,
    RETURN_RUNTIME_ERROR,
    RETURN_PARSE_ERRROR,
    RETURN_INCORRECT_PASSWORD,
    RETURN_HOST_KEY_UNKNOWN,
    RETURN_HOST_KEY_CHANGED,
};


int runprogram( int argc, char *argv[] );

struct {
    enum { PWT_STDIN, PWT_FILE, PWT_FD, PWT_PASS } pwtype;
    union {
	const char *filename;
	int fd;
	const char *password;
    } pwsrc;
} args;

static void print_help()
{
    printf("Usage:  pwdfiller  [-f|-d|-p|-e] [-hV] command parameters\n"
	    "   -f filename   Take password to use from file\n"
	    "   -d number     Use number as file descriptor for getting password\n"
	    "   -p password   Provide password as argument (security unwise)\n"
	    "   -e            Password is passed as env-var \"SSHPASS\"\n"
	    "   With no parameters - password will be taken from stdin\n\n"
	    "   -h            Show help (this screen)\n"
	    "   -V            Print version information\n"
	    "At most one of -f, -d, -p or -e should be used\n");
}

// Parse the command line. Fill in the "args" global struct with the results. Return argv offset
// on success, and a negative number on failure
static int parse_options( int argc, char *argv[] )
{
    int error=-1;
    int opt;

    // Set the default password source to stdin
    args.pwtype=PWT_STDIN;
    args.pwsrc.fd=0;

    while( (opt=getopt(argc, argv, "+f:d:p:heV"))!=-1 && error==-1 ) {
	switch( opt ) {
	case 'f':
	    // Password should come from a file	    
	    
	    args.pwtype=PWT_FILE;
	    args.pwsrc.filename=optarg;
	    break;
	case 'd':
	    // Password should come from an open file descriptor

	    args.pwtype=PWT_FD;
	    args.pwsrc.fd=atoi(optarg);
	    break;
	case 'p':
	    // Password is given on the command line

	    args.pwtype=PWT_PASS;
	    args.pwsrc.password=strdup(optarg);
            
            // Hide the original password from the command line
            {
                int i;

                for( i=0; optarg[i]!='\0'; ++i )
                    optarg[i]='z';
            }
	    break;
	case 'e':

	    args.pwtype=PWT_PASS;
	    args.pwsrc.password=getenv("SSHPASS");
            if( args.pwsrc.password==NULL ) {
                fprintf(stderr, "sshpass: -e option given but SSHPASS environment variable not set\n");

                error=RETURN_INVALID_ARGUMENTS;
            }
	    break;
	case '?':
	case ':':
	    error=RETURN_INVALID_ARGUMENTS;
	    break;
	case 'h':
	    error=RETURN_NOERROR;
	    break;
	}
    }

    if( error>=0 )
	return -(error+1);
    else
	return optind;
}

int main( int argc, char *argv[] )
{
    int opt_offset=parse_options( argc, argv );

    if( opt_offset<0 ) {
	// There was some error
	print_help();

        return -(opt_offset+1); // -1 becomes 0, -2 becomes 1 etc.
    }

    if( argc-opt_offset<1 ) {
	print_help();

        return 0;
    }

    return runprogram( argc-opt_offset, argv+opt_offset );
}

int handleoutput( int fd );

/* Global variables so that this information be shared with the signal handler */
static int ourtty; // Our own tty
static int masterpt;

void window_resize_handler(int signum);
void sigchld_handler(int signum);

int runprogram( int argc, char *argv[] )
{
    struct winsize ttysize; // The size of our tty

    // We need to interrupt a select with a SIGCHLD. In order to do so, we need a SIGCHLD handler
    signal( SIGCHLD,sigchld_handler );

    // Create a pseudo terminal for our process
    masterpt=posix_openpt(O_RDWR);

    if( masterpt==-1 ) {
		perror("Failed to get a pseudo terminal");

		return RETURN_RUNTIME_ERROR;
    }

    fcntl(masterpt, F_SETFL, O_NONBLOCK);

    if( grantpt( masterpt )!=0 ) {
		perror("Failed to change pseudo terminal's permission");

		return RETURN_RUNTIME_ERROR;
    }
    if( unlockpt( masterpt )!=0 ) {
		perror("Failed to unlock pseudo terminal");

		return RETURN_RUNTIME_ERROR;
    }

    ourtty=open("/dev/tty", 0);
    if( ourtty!=-1 && ioctl( ourtty, TIOCGWINSZ, &ttysize )==0 ) {
        signal(SIGWINCH, window_resize_handler);

        ioctl( masterpt, TIOCSWINSZ, &ttysize );
    }

    const char *name=ptsname(masterpt);
    int slavept;

    int childpid=fork();	
    if( childpid==0 ) {
		// Child

		// Detach us from the current TTY
		setsid();
		
		// This line makes the ptty our controlling tty. We do not otherwise need it open
		slavept=open(name, O_RDWR );
		close( slavept );
		
		close( masterpt );

		char **new_argv=malloc(sizeof(char *)*(argc+1));

		int i;

		for( i=0; i<argc; ++i ) {
			new_argv[i]=argv[i];
		}

		new_argv[i]=NULL;

		execvp( new_argv[0], new_argv );

		perror("sshpass: Failed to run command");

		exit(RETURN_RUNTIME_ERROR);
    } else if( childpid<0 ) {
		perror("sshpass: Failed to create child process");

		return RETURN_RUNTIME_ERROR;
    }
	
    // We are the parent
    slavept=open(name, O_RDWR|O_NOCTTY );

    int status=0;
    int terminate=0;
    pid_t wait_id;
    sigset_t sigmask, sigmask_select;

    // Set the signal mask during the select
    sigemptyset(&sigmask_select);

    // And during the regular run
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGCHLD);

    sigprocmask( SIG_SETMASK, &sigmask, NULL );

    do {
		if( !terminate ) {
			fd_set readfd;

			FD_ZERO(&readfd);
			FD_SET(masterpt, &readfd);

			int selret=pselect( masterpt+1, &readfd, NULL, NULL, NULL, &sigmask_select );

			if( selret>0 ) {
				if( FD_ISSET( masterpt, &readfd ) ) {
							int ret;
					if( (ret=handleoutput( masterpt )) ) {
					// Authentication failed or any other error

								// handleoutput returns positive error number in case of some error, and a negative value
								// if all that happened is that the slave end of the pt is closed.
								if( ret>0 ) {
									close( masterpt ); // Signal ssh that it's controlling TTY is now closed
									close(slavept);
								}

					terminate=ret;

								if( terminate ) {
									close( slavept );
								}
					}
				}
			}
			wait_id=waitpid( childpid, &status, WNOHANG );
		} else {
			wait_id=waitpid( childpid, &status, 0 );
		}
    } while( wait_id==0 || (!WIFEXITED( status ) && !WIFSIGNALED( status )) );

    if( terminate>0 )
		return terminate;
    else if( WIFEXITED( status ) )
		return WEXITSTATUS(status);
    else
		return 255;
}

int match( const char *reference, const char *buffer, ssize_t bufsize, int state );
void write_pass( int fd );

int handleoutput( int fd )
{
    // We are looking for the string
    static int prevmatch=0; // If the "password" prompt is repeated, we have the wrong password.
    static int state1, state2;
    static const char compare1[]="assword:"; // Asking for a password
    static const char compare2[]="The authenticity of host "; // Asks to authenticate host
    // static const char compare3[]="WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!"; // Warns about man in the middle attack
    // The remote identification changed error is sent to stderr, not the tty, so we do not handle it.
    // This is not a problem, as ssh exists immediately in such a case
    char buffer[40];
    int ret=0;

    int numread=read(fd, buffer, sizeof(buffer) );
perror(buffer);
    state1=match( compare1, buffer, numread, state1 );

    // Are we at a password prompt?
    if( compare1[state1]=='\0' ) {
	if( !prevmatch ) {
	    write_pass( fd );
	    state1=0;
	    prevmatch=1;
	} else {
	    // Wrong password - terminate with proper error code
	    ret=RETURN_INCORRECT_PASSWORD;
	}
    }

    if( ret==0 ) {
        state2=match( compare2, buffer, numread, state2 );

        // Are we being prompted to authenticate the host?
        if( compare2[state2]=='\0' ) {
            ret=RETURN_HOST_KEY_UNKNOWN;
        }
    }

    return ret;
}

int match( const char *reference, const char *buffer, ssize_t bufsize, int state )
{
    // This is a highly simplisic implementation. It's good enough for matching "Password: ", though.
    int i;
    for( i=0;reference[state]!='\0' && i<bufsize; ++i ) {
	if( reference[state]==buffer[i] )
	    state++;
	else {
	    state=0;
	    if( reference[state]==buffer[i] )
		state++;
	}
    }

    return state;
}

void write_pass_fd( int srcfd, int dstfd );

void write_pass( int fd )
{
    switch( args.pwtype ) {
    case PWT_STDIN:
	write_pass_fd( STDIN_FILENO, fd );
	break;
    case PWT_FD:
	write_pass_fd( args.pwsrc.fd, fd );
	break;
    case PWT_FILE:
	{
	    int srcfd=open( args.pwsrc.filename, O_RDONLY );
	    if( srcfd!=-1 ) {
		write_pass_fd( srcfd, fd );
		close( srcfd );
	    }
	}
	break;
    case PWT_PASS:
	write( fd, args.pwsrc.password, strlen( args.pwsrc.password ) );
	write( fd, "\n", 1 );
	break;
    }
}

void write_pass_fd( int srcfd, int dstfd )
{

    int done=0;

    while( !done ) {
	char buffer[40];
	int i;
	int numread=read( srcfd, buffer, sizeof(buffer) );
	done=(numread<1);
	for( i=0; i<numread && !done; ++i ) {
	    if( buffer[i]!='\n' )
		write( dstfd, buffer+i, 1 );
	    else
		done=1;
	}
    }

    write( dstfd, "\n", 1 );
}

void window_resize_handler(int signum)
{
    struct winsize ttysize; // The size of our tty

    if( ioctl( ourtty, TIOCGWINSZ, &ttysize )==0 )
        ioctl( masterpt, TIOCSWINSZ, &ttysize );
}

// Do nothing handler - makes sure the select will terminate if the signal arrives, though.
void sigchld_handler(int signum)
{
}
