#!/bin/bash

export HISTIGNORE="*"

FEXEC_HOME="/home/mike/my_dev/libssh2/fexec";
FEXEC_DATA="/home/mike/fexec/data";
FEXEC_TEMP="/home/mike/fexec/temp";
FEXEC_AGENT="$FEXEC_HOME/agent/fexec_agent.exe";
FEXEC_VERSION="v1";

[ ! -d "$FEXEC_DATA" ] && mkdir -p "$FEXEC_DATA" >/dev/null 2>&1
[ ! -d "$FEXEC_TEMP" ] && mkdir -p "$FEXEC_TEMP" >/dev/null 2>&1

function fexec_init()
{
	SERVER_HOST=$1;
	USERNAME=$2
	PASSWORD=$3
	
	#FEXEC_UUID=`cat /proc/sys/kernel/random/uuid`;
	local HOSTNAME=`hostname`;

	TARGET_SVRNAME="fexec_$FEXEC_VERSION_$HOSTNAME";
	TARGET_VOLUME="c"
	TARGET_FOLDER="fexec_$HOSTNAME";
	TARGET_FILENAME="fexec_agent.exe"
	TARGET_PATH="$TARGET_VOLUME:\\$TARGET_FOLDER\\$TARGET_FILENAME"
	TARGET_SHARE="//${SERVER_HOST}/${TARGET_VOLUME}\$";


	CMD_SMB="smbclient ${TARGET_SHARE} -U ${USERNAME}"
	CMD_NET="net rpc service"
}

function start_agent()
{
	local RC="";
	local SERVER_HOST=$1;
	
	RC=`$CMD_NET create ${TARGET_SVRNAME} ${TARGET_SVRNAME} "$TARGET_PATH" -I ${SERVER_HOST} -U ${USERNAME} 2>&1 0<<<"${PASSWORD}"`
	if [ "$?" != "0" ]; then
		RC=`echo "$RC" | grep WERR_SERVICE_EXISTS`;
		[ -z "$RC" ] && return 1;
	fi
	
	$CMD_NET start ${TARGET_SVRNAME} -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 2>&1 0<<<"${PASSWORD}"
	[ "$?" != "0" ] && return 2;
	
	return 0;
}

function stop_agent()
{
	local SERVER_HOST=$1;
	
	$CMD_NET stop "${TARGET_SVRNAME}" -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 0<<<"${PASSWORD}"
	[ "$?" != "0" ] && return 1;
	
	$CMD_NET delete "${TARGET_SVRNAME}" -I ${SERVER_HOST} -U ${USERNAME} 2>/dev/null 0<<<"${PASSWORD}"
	[ "$?" != "0" ] && return 2;
	
	return 0;
}

function install_agent()
{
	local RC=""
	#local SERVER_HOST=$1;	
	
		
		local CMD="mkdir ${TARGET_FOLDER}; setmode ${TARGET_FOLDER} +hs; put ${FEXEC_AGENT} ${TARGET_FOLDER}\\${TARGET_FILENAME}; ls ${TARGET_FOLDER}\\${TARGET_FILENAME};"
		RC=`$CMD_SMB -c "${CMD}" 2>/dev/null 0<<<"$PASSWORD"`
		[ "$?" != "0" ] && return 1;
		RC=`echo "$RC" | grep NT_STATUS_NO_SUCH_FILE`
		[ -n "$RC" ] && return 1;
		
		start_agent "$SERVER_HOST";
		if [ "$?" != "0" ]; then
			uninstall_agent "$SERVER_HOST" "NO_NEED_STOP"
			return 2;
		fi
	
	
	return 0;
}

function uninstall_agent()
{
	local SERVER_HOST=$1;
	local NO_NEED_STOP=$2;
	
	if [ -z "$NO_NEED_STOP" ]; then
		stop_agent "$SERVER_HOST";
		[ "$?" != "0" ] && return 1;
	fi
	
	local CMD="rm ${TARGET_FOLDER}\\*; rmdir ${TARGET_FOLDER}"
	$CMD_SMB -c "${CMD}" >/dev/null 2>&1 0<<<"${PASSWORD}"
	return $?;
}

function exec_script()
{
	local SCRIPT=$1;
	local UUID=`cat /proc/sys/kernel/random/uuid`;
	local CMD_PUT="put ${FEXEC_TEMP}/fexec_$UUID.bat ${TARGET_FOLDER}\\_fexec_$UUID.bat; rename ${TARGET_FOLDER}\\_fexec_$UUID.bat ${TARGET_FOLDER}\\fexec_$UUID.bat;";
	local CMD_GET="get ${TARGET_FOLDER}\\fexec_$UUID.out \"${FEXEC_TEMP}/fexec_$UUID.out\"; rm ${TARGET_FOLDER}\\fexec_$UUID.out;";
	
	echo "$SCRIPT" > "${FEXEC_TEMP}/fexec_$UUID.bat"
	
	$CMD_SMB -c "${CMD_PUT}" >/dev/null 2>&1 0<<<"${PASSWORD}"
	[ "$?" != "0" ] && return 1;
	
	while(true)
	do
	{
		RC=`$CMD_SMB -c "${CMD_GET}" 2>/dev/null 0<<<"${PASSWORD}" | grep NT_STATUS_OBJECT_NAME_NOT_FOUND`
		[ -z "$RC" ] && break;
		sleep 1
	}
	done;
	
	cat "${FEXEC_TEMP}/fexec_$UUID.out";
	rm -f "${FEXEC_TEMP}/fexec_$UUID.out";
	rm -f "${FEXEC_TEMP}/fexec_$UUID.bat";
	return 0;
}


#main entry
HOST=$1
USERNAME=$2
SCRIPT=$3

fexec_init "$HOST" "$USERNAME"


	(
		flock -x 10;						#wait until get an exclusive lock
		[ "$?" != "0" ] && exit 1;
		
		install_agent "$HOST"
		[ "$?" != "0" ] && exit 1;
	) 10<>$FEXEC_TEMP/lock.lck
	
	(
		flock -s -w 30 9;						#need shared lock and keep it for the whole process
		
		exec_script "$SCRIPT"
		[ "$?" != "0" ] && exit 1;
	) 9<>$FEXEC_TEMP/lock.lck

	(
		flock -x -n 10;					#uninstall only after get an exclusive lock
		if [ "$?" = "0" ]; then
			uninstall_agent "$HOST"
			[ "$?" != "0" ] && exit 1;
		fi
	) 10<>$FEXEC_TEMP/lock.lck


