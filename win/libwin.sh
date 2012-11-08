export HISTIGNORE="*"
CMD_SMB="pwdfiller smbclient"
CMD_NET="pwdfiller net rpc service"

TARGET_VOLUME="c"
TARGET_FILENAME="fexec_agent.exe"
TARGET_PATH="$TARGET_VOLUME:\\fexec\\$TARGET_FILENAME"
TARGET_SERVICE="//${SERVER_HOST}/${TARGET_VOLUME}\$";


function start_agent()
{
	local RC="";
	local SERVER_HOST=$1;
	
	`$CMD_NET fexec fexec "$TARGET_PATH" -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 0<<<"${PASSWORD}"`
	[ "$?" != "0" ] && return 1;
	
	`$CMD_NET start fexec -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 0<<<"${PASSWORD}"`
	[ "$?" != "0" ] && return 2;
	
	return 0;
}

function stop_agent()
{
	local SERVER_HOST=$1;
	
	`$CMD_NET stop fexec -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 0<<<"${PASSWORD}"`
	[ "$?" != "0" ] && return 1;
	
	`$CMD_NET delete fexec -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 0<<<"${PASSWORD}"`
	[ "$?" != "0" ] && return 2;
	
	return 0;
}

function install_agent()
{
	local RC=""
	local SERVER_HOST=$1;
	local CMD="mkdir fexec; setmode fexec +hs; put ${TARGET_FILENAME} \\fexec\\${TARGET_FILENAME}; ls \\fexec\\${TARGET_FILENAME};"
	RC=`$CMD_SMB ${TARGET_SERVICE} -U ${USERNAME} -c "${CMD}" 2>/dev/null 0<<<"$PASSWORD" | grep NT_STATUS_NO_SUCH_FILE`
	[ -n "$RC" ] && return 1;
	
	start_agent "$SERVER_HOST";
	[ "$?" != "0" ] && return 2;
	
	return 0;
}

function uninstall_agent()
{
	local SERVER_HOST=$1;
	
	stop_agent "$SERVER_HOST";
	[ "$?" != "0" ] && return 1;
	
	local CMD="rm \\fexec\\*; rmdir \\fexec"
	`$CMD_SMB ${TARGET_SERVICE} -U ${USERNAME} -c "${CMD}" 2>/dev/null 0<<<"${PASSWORD}"`
	return $?;
}

function exec_script()
{
	local SCRIPT=$1;
	local CMD_PUT="put _sin \\fexec\_fin; rename \\fexec\_fin \\fexec\fin;";
	local CMD_GET="get \\fexec\fout";

}
