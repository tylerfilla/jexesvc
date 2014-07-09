
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>

#include <stdio.h>
#include <stdlib.h>

#include "util.h"

char* recvline(SOCKET socket) {
    size_t lenmax = 64;
    size_t len = lenmax;
    
    char* line = calloc(len, sizeof(char));
    
    char c = 0;
    int i = 0;
    while (1) {
        char buf[1];
        int result = recv(socket, buf, 1, 0);
        if (result > 0) {
            c = buf[0];
            if (c != '\n' && c != EOF) {
                if (c != '\r') {
                    line[i++] = c;
                    if (i == len) {
                        len += lenmax;
                        line = realloc(line, len);
                        
                        for (int j = i; j < len; j++) {
                            line[j] = 0;
                        }
                    }
                }
            } else {
                break;
            }
        } else if (result == 0) {
            return NULL;
        }
    }
    
    return line;
}
