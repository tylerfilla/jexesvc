
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "main.h"
#include "versioning.h"

SERVICE_STATUS        serviceStatus;
SERVICE_STATUS_HANDLE serviceStatusHandle;
HANDLE                serviceStopEventHandle;

int debugMode;
int debugModeStop;

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
            return jexesvcMain();
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
    return jexesvcMain();
}

void serviceUpdateStatus() {
    if (!SetServiceStatus(serviceStatusHandle, &serviceStatus)) {
        OutputDebugString("Unable to update service status");
    }
}

/* JEXESVC Functions */

int jexesvcMain() {
    if (debugMode) {
        SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
        
        printf("JEXESVC %s (%d) \n\n", VERSION_FULLVERSION_STRING, (int) VERSION_BUILDS_COUNT);
        printf("This service is running as a normal console application.\n");
        printf("Use this mode only for debugging purposes.\n\n");
    }
    
    HANDLE cmdPipe;
    
    while (shouldContinue()) {
        cmdPipe = CreateNamedPipe("\\\\.\\pipe\\jexesvc\\cmd", PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT, PIPE_UNLIMITED_INSTANCES, 512, 512, 0, NULL);
        
        if (cmdPipe == INVALID_HANDLE_VALUE) {
            if (debugMode) {
                printf("Unable to create command pipe instance (Error %d)\n", GetLastError());
            }
            break;
        }
        
        if (ConnectNamedPipe(cmdPipe, NULL)) {
            if (debugMode) {
                printf("Accepted connection to command pipe\n");
            }
            CreateThread(NULL, 0, clientThread, cmdPipe, 0, NULL);
        } else {
            CloseHandle(cmdPipe);
        }
    }
    
    if (debugMode) {
        printf("JEXESVC is exiting...\n");
    }
    
    CloseHandle(cmdPipe);
    
    return 0;
}

DWORD WINAPI clientThread(LPVOID lpvParam) {
    HANDLE cmdPipe = (HANDLE) lpvParam;
    
    // TODO: Handle client
    
    CloseHandle(cmdPipe);
    
    return 0;
}

BOOL WINAPI consoleCtrlHandler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_CLOSE_EVENT) {
        debugModeStop = 1;
        return TRUE;
    }
    return FALSE;
}

int shouldContinue() {
    if (debugMode) {
        return !debugModeStop;
    } else {
        return WaitForSingleObject(serviceStopEventHandle, 0);
    }
    return 0;
}
