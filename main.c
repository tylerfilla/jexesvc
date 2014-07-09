
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>

#include <stdio.h>
#include <stdlib.h>

#include "versioning.h"

#define SERVICE_NAME "JEXESVC"

#define RUNTIME_CONTEXT_SERVICE 0
#define RUNTIME_CONTEXT_CONSOLE 1

#define SERVER_PORT 1337

VOID  WINAPI ServiceMain(DWORD, LPSTR*);
VOID  WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceThread(LPVOID);
BOOL         updateStatus();
int          listenLoop();
void         handleClient();
char* recvline(SOCKET socket);

SERVICE_STATUS        g_ServiceStatus;
SERVICE_STATUS_HANDLE g_StatusHandle;
HANDLE                g_ServiceStopEvent;

int runtimeContext;

int main(int argc, char* argv[]) {
    runtimeContext = RUNTIME_CONTEXT_SERVICE;
    
    SERVICE_TABLE_ENTRY ServiceTableEntry[] = {
        {
            (char*)                   SERVICE_NAME,
            (LPSERVICE_MAIN_FUNCTION) ServiceMain,
        },
        {
            NULL,
            NULL,
        },
    };
    
    if (!StartServiceCtrlDispatcher(ServiceTableEntry)) {
        int error = GetLastError();
        if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            runtimeContext = RUNTIME_CONTEXT_CONSOLE;
            return listenLoop();
        } else {
            return error;
        }
    }
    
    return 0;
}

VOID WINAPI ServiceMain(DWORD argc, LPSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_StatusHandle) {
        return;
    }
    
    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwWin32ExitCode = 0;
    updateStatus();
    
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_ServiceStopEvent) {
        g_ServiceStatus.dwCheckPoint = 1;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        updateStatus();
        
        return;
    }
    
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    updateStatus();
    
    WaitForSingleObject(CreateThread(NULL, 0, ServiceThread, NULL, 0, NULL), INFINITE);
    
    CloseHandle(g_ServiceStopEvent);
    
    g_ServiceStatus.dwCheckPoint = 3;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    updateStatus();
}

VOID WINAPI ServiceCtrlHandler(DWORD ctrlCode) {
    switch (ctrlCode) {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING) {
            break;
        }
        
        g_ServiceStatus.dwCheckPoint = 4;
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        updateStatus();
        
        SetEvent(g_ServiceStopEvent);
        
        break;
    }
}

DWORD WINAPI ServiceThread(LPVOID lpParam) {
    return listenLoop();
}

BOOL updateStatus() {
    if (!SetServiceStatus(g_StatusHandle, &g_ServiceStatus)) {
        OutputDebugString("Unable to update service status");
        return FALSE;
    }
    
    return TRUE;
}

int listenLoop() {
    if (runtimeContext == RUNTIME_CONTEXT_CONSOLE) {
        printf("JEXESVC %s (%d) \n\n", VERSION_FULLVERSION_STRING, (int) VERSION_BUILDS_COUNT);
        printf("This service is running as a normal console application.\n");
        printf("Use this mode only for debugging purposes.\n\n");
    }
    
    int ready = 1;
    
    WSADATA wsaData;
    
    SOCKET socketServer;
    SOCKET socketClient;
    
    struct sockaddr_in addrServer;
    struct sockaddr_in addrClient;
    
    int addrServerSize = sizeof(addrClient);
    int addrClientSize = sizeof(addrClient);
    
    addrServer.sin_addr.s_addr = INADDR_ANY;
    addrServer.sin_family = AF_INET;
    addrServer.sin_port = htons(SERVER_PORT);
    
    if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        if (runtimeContext == RUNTIME_CONTEXT_CONSOLE) {
            printf("WSAStartup failed (Error %d)\n", WSAGetLastError());
        }
        return 1;
    }
    
    if ((socketServer = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        if (runtimeContext == RUNTIME_CONTEXT_CONSOLE) {
            printf("Unable to create server socket (Error %d)\n", WSAGetLastError());
            ready = 0;
        }
    }
    
    if (bind(socketServer, (struct sockaddr*) &addrServer, addrServerSize) == SOCKET_ERROR) {
        if (runtimeContext == RUNTIME_CONTEXT_CONSOLE) {
            printf("Unable to bind server socket (Error %d)\n", WSAGetLastError());
            ready = 0;
        }
    }
    
    listen(socketServer, 3);
    
    if (ready) {
        if (runtimeContext == RUNTIME_CONTEXT_CONSOLE) {
            printf("Listening on port %d...\n\n", SERVER_PORT);
        }
        
        while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0) {
            socketClient = accept(socketServer, (struct sockaddr*) &addrClient, &addrClientSize);
            if (socketClient != INVALID_SOCKET) {
                handleClient(socketClient, &addrClient, &addrClientSize);
                closesocket(socketClient);
            } else {
                if (runtimeContext == RUNTIME_CONTEXT_CONSOLE) {
                    printf("Client socket is invalid\n");
                }
            }
        }
    }
    
    closesocket(socketServer);
    WSACleanup();
    
    return 0;
}

void handleClient(SOCKET socket, struct sockaddr_in* addr, int addrSizePtr) {
    if (runtimeContext == RUNTIME_CONTEXT_CONSOLE) {
        printf("New connection from %s:%d\n", inet_ntoa(addr->sin_addr), addr->sin_port);
    }
    
    while (1) {
        char* line = recvline(socket);
        
        if (line == NULL) {
            if (runtimeContext == RUNTIME_CONTEXT_CONSOLE) {
                printf("Lost connection to %s:%d\n", inet_ntoa(addr->sin_addr), addr->sin_port);
            }
            break;
        }
        
        send(socket, line, strlen(line), 0);
        
        free(line);
    }
}

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
        } else if (result == 0) {
            return NULL;
        }
    }
    
    return line;
}
