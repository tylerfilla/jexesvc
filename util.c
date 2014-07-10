
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

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
