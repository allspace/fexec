export HISTIGNORE="*"

FEXEC_HOME="/home/mike/fexec"
FEXEC_DATA="$FEXEC_HOME/data"

FEXEC_UUID="";

TARGET_SVRNAME="fexec_$FEXEC_UUID";
TARGET_VOLUME="c"
TARGET_FOLDER="fexec_$FEXEC_UUID";
TARGET_FILENAME="fexec_agent.exe"
TARGET_PATH="$TARGET_VOLUME:\\$TARGET_FOLDER\\$TARGET_FILENAME"
TARGET_SERVICE="//${SERVER_HOST}/${TARGET_VOLUME}\$";


CMD_SMB="pwdfiller smbclient ${TARGET_SERVICE} -U ${USERNAME}"
CMD_NET="pwdfiller net rpc service"

function start_agent()
{
	local RC="";
	local SERVER_HOST=$1;
	
	`$CMD_NET create ${TARGET_SVRNAME} fexec "$TARGET_PATH" -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 0<<<"${PASSWORD}"`
	[ "$?" != "0" ] && return 1;
	
	`$CMD_NET start ${TARGET_SVRNAME} -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 0<<<"${PASSWORD}"`
	[ "$?" != "0" ] && return 2;
	
	return 0;
}

function stop_agent()
{
	local SERVER_HOST=$1;
	
	`$CMD_NET stop "${TARGET_SVRNAME}" -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 0<<<"${PASSWORD}"`
	[ "$?" != "0" ] && return 1;
	
	`$CMD_NET delete "${TARGET_SVRNAME}" -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 0<<<"${PASSWORD}"`
	[ "$?" != "0" ] && return 2;
	
	return 0;
}

function install_agent()
{
	local RC=""
	local SERVER_HOST=$1;
	FEXEC_UUID=`cat /proc/sys/kernel/random/uuid`;
	local CMD="mkdir ${TARGET_FOLDER}; setmode ${TARGET_FOLDER} +hs; put ${TARGET_FILENAME} ${TARGET_FOLDER}\\${TARGET_FILENAME}; ls ${TARGET_FOLDER}\\${TARGET_FILENAME};"
	RC=`$CMD_SMB -c "${CMD}" 2>/dev/null 0<<<"$PASSWORD" | grep NT_STATUS_NO_SUCH_FILE`
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
	
	local CMD="rm ${TARGET_FOLDER}\\*; rmdir ${TARGET_FOLDER}"
	`$CMD_SMB -c "${CMD}" 2>/dev/null 0<<<"${PASSWORD}"`
	return $?;
}

function exec_script()
{
	local SCRIPT=$1;
	local CMD_PUT="put _sin ${TARGET_FOLDER}\\_fin; rename ${TARGET_FOLDER}\\_fin ${TARGET_FOLDER}\\fin;";
	local CMD_GET="get ${TARGET_FOLDER}\\fout /tmp/$UUID.out; rm ${TARGET_FOLDER}\\fout;";
	
	`$CMD_SMB -c "${CMD_PUT}" 2>/dev/null 0<<<"${PASSWORD}"`
	[ "$?" != "0" ] && return 1;
	
	while(true)
	do
	{
		RC=`$CMD_SMB -c "${CMD_GET}" 2>/dev/null 0<<<"${PASSWORD}" | grep NT_STATUS_OBJECT_NAME_NOT_FOUND`
		[ -z "$RC" ] && break;
		sleep 1
	}
	done;
	
	cat /tmp/$UUID.out;
	return 0;
}
