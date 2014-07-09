
#ifndef _MAIN_H_
#define _MAIN_H_

/* Type Definitions */

typedef struct {
    SOCKET socket;
    struct sockaddr_in address;
    int addressStructSize;
} netnode_t;

typedef struct {
    char* username;
    char* password;
} login_t;

/* Function Prototypes */

VOID  WINAPI serviceMain(DWORD, LPSTR*);
VOID  WINAPI serviceCtrlHandler(DWORD);
DWORD WINAPI serviceThread(LPVOID);
void         serviceUpdateStatus();
int          listenLoop();
DWORD WINAPI clientHandlerThread(LPVOID);
char*        handleClientCommand(char*);

#endif // _MAIN_H_
