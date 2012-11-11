/*
Copyright (c) 2012-2013, allspace@gmail.com.
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions 
are met:

    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.

    * Neither the name of allspace@gmail.com, Inc. nor the names of 
      its contributors may be used to endorse or promote products 
      derived from this software without specific prior written 
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
SUCH DAMAGE.
*/

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <tchar.h>

#define LOGFILE "fexec_agent.log"
#define SERVICE_NAME L"fexec_agent"
#define VERSION "1.0"

SERVICE_STATUS ServiceStatus; 
SERVICE_STATUS_HANDLE hStatus; 
 
void  ServiceMain(int argc, char** argv); 
void  ControlHandler(DWORD request); 


int WriteToLog(TCHAR* str)
{
	errno_t rc = 0;
	FILE* log = NULL;

	rc = fopen_s(&log, LOGFILE, "a+");
	if (rc != 0)
		return -1;
	_ftprintf(log, _TEXT("%s\n"), str);
	fclose(log);
	return 0;
}

void main() 
{ 
    SERVICE_TABLE_ENTRY ServiceTable[2];
    ServiceTable[0].lpServiceName = (SERVICE_NAME);
    ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

    ServiceTable[1].lpServiceName = NULL;
    ServiceTable[1].lpServiceProc = NULL;
    // Start the control dispatcher thread for our service
    StartServiceCtrlDispatcher(ServiceTable);  
}

int GetRootPath(TCHAR *rootPath, DWORD nSize)
{
	DWORD rc = 0;
	TCHAR modName[1024];
	TCHAR* lpPart={NULL};

	rc = GetModuleFileName(NULL, modName, _countof(modName));
	if(rc==0 || rc==nSize) return -1;
	
	rc = GetFullPathName(modName, nSize, rootPath, &lpPart);
	if(rc==0) return -1;
	*(lpPart-1) = _T('\0');
	//WriteToLog(rootPath);
	return 0;					
	//DWORD test = WaitForSingleObject( h, 1 );
}

int fexec_proc(TCHAR *rootPath, const TCHAR *file)
{
	int rc = 0;
	TCHAR fileName[1024];
	TCHAR cmd[1024];
	TCHAR out[1024];
	
	_tcsncpy_s(fileName, _countof(fileName), file, _TRUNCATE);

	for(int i=0; fileName[i]!=_T('\0'); i++) 
	{
		if(fileName[i]==_T('.')) {fileName[i]=_T('\0'); break;}
	}
	
	_stprintf_s(cmd, _countof(cmd),_TEXT("%s\\%s.bat > %s\\%s.out 2>&1"), rootPath, fileName, rootPath, fileName);
	_stprintf_s(out, _countof(out),_TEXT("%s\\%s.out"), rootPath, fileName);
	
	WriteToLog(cmd);

	rc = _tsystem(cmd);
	if(_taccess_s(out, 00)!=0)
	{
		FILE *fp = NULL;
		rc = _tfopen_s(&fp, out, _TEXT("w"));
		if(rc==0)
		{
			fputs("FEXEC_COMMAND_FAILURE", fp);		//here must be in ANSI
			fclose(fp);
		}
	}
	DeleteFile(file);
	return 0;
}

int fexec_main(TCHAR *rootPath)
{
	DWORD rc = 0;
	WIN32_FIND_DATA wfd;	
	HANDLE hd = FindFirstChangeNotification(
					rootPath,
					TRUE,
					FILE_NOTIFY_CHANGE_FILE_NAME);
	if(hd==NULL || hd== INVALID_HANDLE_VALUE)
	{
		WriteToLog((rootPath));
		return NULL;
	}
	
	while(ServiceStatus.dwCurrentState == SERVICE_RUNNING)
	{
		rc = WaitForSingleObject(hd, 2000/*INFINITE*/);
		if(rc==WAIT_FAILED) break;
		if(rc!=WAIT_OBJECT_0) continue;	

		TCHAR file[1024];
		//check if there is script execute request
		_stprintf_s(file, _countof(file), _TEXT("%s\\fexec_*.bat"), rootPath);
		HANDLE fd = FindFirstFile(file, &wfd);
		if(fd!=INVALID_HANDLE_VALUE)
		{
			do{
				WriteToLog(wfd.cFileName);
				rc = fexec_proc(rootPath, wfd.cFileName);				
			}while(FindNextFile(fd, &wfd)!=0);
		}
		FindClose(fd);

		FindNextChangeNotification(hd);

		//check if there is stop service request
		_stprintf_s(file, _countof(file), _TEXT("%s\\fexec-cmd-stop"), rootPath);
		if(_taccess_s(file, 00)==0)
		{
			DeleteFile(file);
			break;
		}
	}

	FindCloseChangeNotification(hd);

	return 0;
}


void ServiceMain(int argc, char** argv) 
{  
    ServiceStatus.dwServiceType        = SERVICE_WIN32; 
    ServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
    ServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    ServiceStatus.dwWin32ExitCode      = 0; 
    ServiceStatus.dwServiceSpecificExitCode = 0; 
    ServiceStatus.dwCheckPoint         = 0; 
    ServiceStatus.dwWaitHint           = 0; 
 
    hStatus = RegisterServiceCtrlHandler(
		SERVICE_NAME, 
		(LPHANDLER_FUNCTION)ControlHandler); 
    if (hStatus == (SERVICE_STATUS_HANDLE)0) 
    { 
        // Registering Control Handler failed
        return; 
    }  

    // Initialize Service 
	DWORD rc = 0;
	TCHAR rootPath[1024];
	rc = GetRootPath(rootPath, _countof(rootPath));
    if (rc<0) 
    {
		// Initialization failed
        ServiceStatus.dwCurrentState       = SERVICE_STOPPED; 
        ServiceStatus.dwWin32ExitCode      = -1; 
        SetServiceStatus(hStatus, &ServiceStatus); 
        return; 
    } 
	SetCurrentDirectory(rootPath);

    // We report the running status to SCM. 
    ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
    SetServiceStatus (hStatus, &ServiceStatus);

	fexec_main(rootPath);

    ServiceStatus.dwWin32ExitCode = 0; 
    ServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
    SetServiceStatus (hStatus, &ServiceStatus);

    return; 
}
 

// Control handler function
void ControlHandler(DWORD request) 
{ 
    switch(request) 
    { 
        case SERVICE_CONTROL_STOP: 
             //WriteToLog(_TEXT("Monitoring stopped."));

            ServiceStatus.dwWin32ExitCode = 0; 
            ServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
            SetServiceStatus (hStatus, &ServiceStatus);
            return; 
 
        case SERVICE_CONTROL_SHUTDOWN: 
            //WriteToLog(_TEXT("Monitoring stopped."));

            ServiceStatus.dwWin32ExitCode = 0; 
            ServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
            SetServiceStatus (hStatus, &ServiceStatus);
            return; 
        
        default:
            break;
    } 
 
    // Report current status
    SetServiceStatus (hStatus,  &ServiceStatus);
 
    return; 
} 

