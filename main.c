
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>

#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "util.h"
#include "versioning.h"

#define SERVICE_NAME "JEXESVC"

#define SERVER_PORT 1337

SERVICE_STATUS        serviceStatus;
SERVICE_STATUS_HANDLE serviceStatusHandle;
HANDLE                serviceStopEventHandle;

int debugMode;
login_t processLogin;

int main(int argc, char** argv) {
    SERVICE_TABLE_ENTRY serviceTableEntry[] = {
        {
            (char*)                   SERVICE_NAME,
            (LPSERVICE_MAIN_FUNCTION) serviceMain,
        },
        {
            NULL,
            NULL,
        },
    };
    
    if (!StartServiceCtrlDispatcher(serviceTableEntry)) {
        int error = GetLastError();
        if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            debugMode = 1;
            return listenLoop();
        } else {
            return error;
        }
    }
    
    return 0;
}

/* Service Functions */

VOID WINAPI serviceMain(DWORD argc, LPSTR* argv) {
    serviceStatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, serviceCtrlHandler);
    if (!serviceStatusHandle) {
        return;
    }
    
    ZeroMemory(&serviceStatus, sizeof(serviceStatus));
    serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serviceUpdateStatus();
    
    serviceStopEventHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!serviceStopEventHandle) {
        serviceStatus.dwCheckPoint = 1;
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        serviceStatus.dwWin32ExitCode = GetLastError();
        serviceUpdateStatus();
        
        return;
    }
    
    serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    serviceStatus.dwCurrentState = SERVICE_RUNNING;
    serviceUpdateStatus();
    
    WaitForSingleObject(CreateThread(NULL, 0, serviceThread, NULL, 0, NULL), INFINITE);
    
    CloseHandle(serviceStopEventHandle);
    
    serviceStatus.dwCheckPoint = 3;
    serviceStatus.dwControlsAccepted = 0;
    serviceStatus.dwCurrentState = SERVICE_STOPPED;
    serviceUpdateStatus();
}

VOID WINAPI serviceCtrlHandler(DWORD control) {
    if (control == SERVICE_CONTROL_STOP) {
        if (serviceStatus.dwCurrentState != SERVICE_RUNNING) {
            return;
        }
        
        serviceStatus.dwCheckPoint = 4;
        serviceStatus.dwControlsAccepted = 0;
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        serviceStatus.dwWin32ExitCode = 0;
        serviceUpdateStatus();
        
        SetEvent(serviceStopEventHandle);
    }
}

DWORD WINAPI serviceThread(LPVOID lpParam) {
    return listenLoop();
}

void serviceUpdateStatus() {
    if (!SetServiceStatus(serviceStatusHandle, &serviceStatus)) {
        OutputDebugString("Unable to update service status");
    }
}

/* JEXESVC Functions */

int listenLoop() {
    if (debugMode) {
        printf("JEXESVC %s (%d) \n\n", VERSION_FULLVERSION_STRING, (int) VERSION_BUILDS_COUNT);
        printf("This service is running as a normal console application.\n");
        printf("Use this mode only for debugging purposes.\n\n");
    }
    
    int ready = 1;
    
    WSADATA wsaData;
    
    netnode_t server;
    server.addressStructSize = sizeof(struct sockaddr_in);
    
    server.address.sin_addr.s_addr = INADDR_ANY;
    server.address.sin_family = AF_INET;
    server.address.sin_port = htons(SERVER_PORT);
    
    if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        if (debugMode) {
            printf("WSAStartup failed (Error %d)\n", WSAGetLastError());
        }
        return 1;
    }
    
    if ((server.socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        if (debugMode) {
            printf("Unable to create server socket (Error %d)\n", WSAGetLastError());
            ready = 0;
        }
    }
    
    if (bind(server.socket, (struct sockaddr*) &server.address, server.addressStructSize) == SOCKET_ERROR) {
        if (debugMode) {
            printf("Unable to bind server socket (Error %d)\n", WSAGetLastError());
            ready = 0;
        }
    }
    
    if (ready) {
        listen(server.socket, 3);
        
        if (debugMode) {
            printf("Listening on port %d...\n\n", SERVER_PORT);
        }
        
        while (WaitForSingleObject(serviceStopEventHandle, 0) != WAIT_OBJECT_0) {
            netnode_t client;
            
            client.addressStructSize = sizeof(struct sockaddr_in);
            client.socket = accept(server.socket, (struct sockaddr*) &client.address, &client.addressStructSize);
            
            if (client.socket != INVALID_SOCKET) {
                CreateThread(NULL, 0, clientHandlerThread, &client, 0, NULL);
            } else {
                if (debugMode) {
                    printf("Client socket is invalid\n");
                }
            }
        }
    }
    
    closesocket(server.socket);
    WSACleanup();
    
    return 0;
}

DWORD WINAPI clientHandlerThread(LPVOID lpParam) {
    netnode_t* client = (netnode_t*) lpParam;
    
    if (debugMode) {
        printf("New connection from %s:%d\n\n", inet_ntoa(client->address.sin_addr), client->address.sin_port);
    }
    
    while (1) {
        char* line = recvline(client->socket);
        
        if (line == NULL) {
            break;
        }
        
        char* response = handleClientCommand(line);
        if (response != NULL) {
            send(client->socket, response, strlen(response), 0);
        }
        
        free(line);
    }
    
    if (debugMode) {
        printf("Disconnected from %s:%d\n\n", inet_ntoa(client->address.sin_addr), client->address.sin_port);
    }
    
    closesocket(client->socket);
    
    return 0;
}

char* handleClientCommand(char* command) {
    char name[32];
    char args[32][256];
    
    memset(name, 0, sizeof(name));
    memset(args, 0, sizeof(args));
    
    int numArgs = 0;
    
    int nameIndex = 0;
    int argsIndex = 0;
    int argsCharIndex = 0;
    
    int stage = 0;
    int quoted = 0;
    
    char c;
    for (int ci = 0; ci < strlen(command); ci++) {
        c = command[ci];
        if (stage == 0) {
            if (c == ' ' || c == '\t') {
                stage = 1;
            } else {
                name[nameIndex++] = c;
            }
        } else if (stage == 1) {
            if ((c == ' ' || c == '\t') && !quoted) {
                if (argsCharIndex > 0) {
                    argsCharIndex = 0;
                    argsIndex++;
                    numArgs++;
                }
            } else {
                if (numArgs == 0) {
                    numArgs = 1;
                }
                if (c == '"') {
                    if (quoted) {
                        quoted = 0;
                    } else {
                        quoted = 1;
                    }
                } else {
                    args[argsIndex][argsCharIndex++] = c;
                }
            }
        }
    }
    
    if (debugMode) {
        printf("Client issued command: \"%s\"\n", command);
        printf("Command name: \"%s\"\n", name);
        printf("Number of arguments: %d\n", numArgs);
        
        for (int i = 0; i < numArgs; i++) {
            printf("Argument #%d: \"%s\"\n", i, args[i]);
        }
        
        printf("\n");
    }
    
    if (!strcmp(name, "exec")) {
        char* executable = args[0];
        int interactive = !strcmp(args[1], "i");
        
        if (debugMode) {
            printf("Executing \"%s\" %s interactivity\n\n", executable, (interactive ? "with" : "without"));
        }
    } else if (!strcmp(name, "kill")) {
    } else if (!strcmp(name, "list")) {
    } else if (!strcmp(name, "info")) {
    } else if (!strcmp(name, "login")) {
        processLogin.username = args[0];
        processLogin.password = args[1];
        
        if (debugMode) {
            printf("Will start process with user \"%s\" and password \"%s\"\n\n", processLogin.username, processLogin.password);
        }
        
        return "OKAY";
    }
    
    return NULL;
}
