
#ifndef _MAIN_H_
#define _MAIN_H_

/* Type Definitions */

typedef struct {
    char* username;
    char* password;
} login_t;

/* Function Prototypes */

VOID  WINAPI serviceMain(DWORD, LPSTR*);
VOID  WINAPI serviceCtrlHandler(DWORD);
DWORD WINAPI serviceThread(LPVOID);
void         serviceUpdateStatus();
int          jexesvcMain();

#endif // _MAIN_H_
