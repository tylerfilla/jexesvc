
#ifndef _MAIN_H_
#define _MAIN_H_

#define SERVICE_NAME "JEXESVC"

#define SERVER_PORT 1337

#define BUFFER_SIZE_PIPE 512
#define BUFFER_SIZE_COMMAND 256

/* Function Prototypes */

VOID  WINAPI serviceMain(DWORD, LPSTR*);
VOID  WINAPI serviceCtrlHandler(DWORD);
DWORD WINAPI serviceThread(LPVOID);
void         serviceUpdateStatus();
int          jexesvcMain();
DWORD WINAPI clientThread(LPVOID);
BOOL  WINAPI consoleCtrlHandler(DWORD);
int          shouldContinue();

#endif // _MAIN_H_
