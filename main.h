
#ifndef _MAIN_H_
#define _MAIN_H_

#define SERVICE_NAME "JEXESVC"

#define PIPE_URL_BASE           "\\\\.\\pipe\\jexesvc\\"
#define PIPE_URL_COMMAND        PIPE_URL_BASE "cmd"
#define PIPE_URL_PROCESS_STDIN  PIPE_URL_BASE "stdin\\"
#define PIPE_URL_PROCESS_STDOUT PIPE_URL_BASE "stdout\\"
#define PIPE_URL_PROCESS_STDERR PIPE_URL_BASE "stderr\\"

#define BUFFER_SIZE_PIPE    512
#define BUFFER_SIZE_COMMAND 256

/* Function Prototypes */

VOID  WINAPI serviceMain(DWORD, LPSTR*);
VOID  WINAPI serviceCtrlHandler(DWORD);
DWORD WINAPI serviceThread(LPVOID);
void         serviceUpdateStatus();
int          jexesvcMain();
BOOL  WINAPI consoleCtrlHandler(DWORD);
int          shouldContinue();
DWORD WINAPI clientThread(LPVOID);
char*        handleRequest(HANDLE, char*);
char*        handleRequestCommand(HANDLE, char*);
char*        handleRequestData(HANDLE, char*);
char*        commandExec(HANDLE, char*);
char*        commandKill(HANDLE, char*);
char*        commandQuery(HANDLE, char*);
char*        commandLogin(HANDLE, char*);

#endif // _MAIN_H_
