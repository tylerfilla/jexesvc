
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
    
    struct sockaddr_in server;
    struct sockaddr_in client;
    
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    
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
    
    if (bind(socketServer, (struct sockaddr*) &server, sizeof(server)) == SOCKET_ERROR) {
        if (runtimeContext == RUNTIME_CONTEXT_CONSOLE) {
            printf("Unable to bind server socket (Error %d)\n", WSAGetLastError());
            ready = 0;
        }
    }
    
    listen(socketServer, 3);
    
    if (runtimeContext == RUNTIME_CONTEXT_CONSOLE) {
        printf("Listening on port %d...\n", SERVER_PORT);
    }
    
    if (ready) {
        while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0) {
            int clientSize = sizeof(client);
            socketClient = accept(socketServer, (struct sockaddr*) &client, &clientSize);
            if (socketClient != INVALID_SOCKET) {
                // TODO: Branch to another thread to handle the socket
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
