
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* readLine(HANDLE handle) {
    size_t lenmax = 64;
    size_t len = lenmax;
    
    char* line = calloc(len, sizeof(char));
    
    char buf[1];
    DWORD numBytesRead;
    
    char c = 0;
    int i = 0;
    
    while (1) {
        if (ReadFile(handle, buf, 1, &numBytesRead, NULL) || GetLastError() == ERROR_MORE_DATA) {
            c = buf[0];
            if (c != '\n' && c != EOF) {
                line[i++] = c;
                if (i == len) {
                    len += lenmax;
                    line = realloc(line, len);
                    
                    for (int j = i; j < len; j++) {
                        line[j] = 0;
                    }
                }
            } else {
                break;
            }
        } else {
            return NULL;
        }
    }
    
    return line;
}

void writeLine(HANDLE handle, char* line) {
    DWORD numBytesWritten;
    WriteFile(handle, line, strlen(line), &numBytesWritten, NULL);
    WriteFile(handle, "\n", 1, &numBytesWritten, NULL);
    FlushFileBuffers(handle);
}

void strcatd(char** str, const char* append) {
    if (*str != NULL && append == NULL) {
        free(*str);
        *str = NULL;
        return;
    }
    
    if (*str == NULL ) {
        *str = calloc(strlen(append) + 1, sizeof(char));
        memcpy(*str, append, strlen(append));
    } else {
        char* tmp = calloc(strlen(*str) + 1, sizeof(char));
        memcpy(tmp, *str, strlen(*str));
        *str = calloc(strlen(*str) + strlen(append) + 1, sizeof(char));
        memcpy(*str, tmp, strlen(tmp));
        memcpy(*str + strlen(*str), append, strlen(append));
        free(tmp);
    }
}

int strstart(char* str, char* prefix) {
    return strstr(str, prefix) == str;
}

int strend(char* str, char* suffix) {
    return strstr(str, suffix) == str + strlen(str) - strlen(suffix);
}

void strsub(char* str, char* sub, int begin, int end) {
    if (begin >= end || end - begin < 0 || end - begin > strlen(str)) {
        return;
    }
    
    int i = 0;
    for (int j = begin; j < end + 1; j++) {
        sub[i++] = str[j];
    }
}

void convertNTPathToWin32Path(char* pathNT, char* pathWin32) {
    char dosTargetPath[64];
    char dosDrive[] = " :";
    for (char c = 'A'; c <= 'Z'; c++) {
        dosDrive[0] = c;
        QueryDosDevice(dosDrive, dosTargetPath, sizeof(dosTargetPath));
        
        if (strlen(dosTargetPath) > 0 && strstart(pathNT, dosTargetPath)) {
            char pathTmp[strlen(pathNT) - strlen(dosTargetPath) + 1];
            strsub(pathNT, pathTmp, strlen(dosTargetPath), strlen(pathNT));
            sprintf(pathWin32, "%s%s", strupr(dosDrive), pathTmp);
            
            break;
        }
    }
}
