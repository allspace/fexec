
TARGET_VOLUME="c"
AGENT_FILENAME="fexec_agent.exe"

function push_agent()
{
	local SERVICE="//${SERVER_HOST}/${TARGET_VOLUME}\$";
	local CMD="mkdir fexec; setmode fexec +hs; put ${AGENT_FILENAME} \\fexec\\${AGENT_FILENAME}"
	`smbclient ${SERVICE} -c ${CMD} -U ${USERNAME}%${PASSWORD}`

}

function clean_agent()
{
	local SERVICE="//${SERVER_HOST}/${TARGET_VOLUME}\$";
	local CMD="rm \\fexec\\*; rmdir \\fexec"
	`smbclient ${SERVICE} -c ${CMD} -U ${USERNAME}%${PASSWORD}`
}

function start_agent()
{
	local RC="";
	
	`net rpc service create fexec fexec "c:\\fexec\\agent.exe" -I ${SERVER_HOST} -U ${USERNAME}%${PASSWORD}`
	RC=$?
	
	`net rpc service start fexec -I ${SERVER_HOST} -U ${USERNAME}%${PASSWORD}`
	RC=$?
	
	return $RC;
}

function stop_agent()
{
	`net rpc service stop fexec -I ${SERVER_HOST} -U ${USERNAME}%${PASSWORD}`
	`net rpc service delete fexec -I ${SERVER_HOST} -U ${USERNAME}%${PASSWORD}`
}

