
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#include "main.h"
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
        printf("JEXESVC %s (%d) \n\n", VERSION_FULLVERSION_STRING, (int) VERSION_BUILDS_COUNT);
        printf("This service is running as a normal console application.\n");
        printf("Use this mode only for debugging purposes.\n\n");
    }
    
    return 0;
}
