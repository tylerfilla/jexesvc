
#ifndef _UTIL_H_
#define _UTIL_H_

/* Function Prototypes */

char* readLine(HANDLE);
void  writeLine(HANDLE, char*);
void  strcatd(char**, const char*);
int   strstart(char*, char*);
int   strend(char*, char*);
void  strsub(char*, char*, int, int);
void  convertNTPathToWin32Path(char*, char*);

#endif // _UTIL_H_
