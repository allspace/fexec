
INC=-I../include
LIB=-L../src/.libs/ -lssh2


OBJS=


rexec : ssh_lib.o ssh2_exec.o 
	gcc -o rexec ssh_lib.o ssh2_exec.o  ${LIB}