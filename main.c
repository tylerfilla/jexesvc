
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "util.h"
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
        cmdPipe = CreateNamedPipe(PIPE_URL_COMMAND, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, BUFFER_SIZE_PIPE, BUFFER_SIZE_PIPE, 0, NULL);
        
        if (cmdPipe == INVALID_HANDLE_VALUE) {
            if (debugMode) {
                printf("Unable to create command pipe instance (Error %d)\n", GetLastError());
            }
            break;
        }
        
        if (ConnectNamedPipe(cmdPipe, NULL)) {
            if (debugMode) {
                printf("%d: Accepted connection to command pipe\n", cmdPipe);
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

DWORD WINAPI clientThread(LPVOID lpvParam) {
    HANDLE cmdPipe = (HANDLE) lpvParam;
    
    while (shouldContinue()) {
        char* request = readLine(cmdPipe);
        
        if (request == NULL) {
            break;
        } else {
            if (debugMode) {
                printf("%d: Received request from client: \"%s\"\n", cmdPipe, request);
            }
            
            char* response = handleRequest(cmdPipe, request);
            if (response == NULL) {
                if (debugMode) {
                    printf("%d: Sending null response\n", cmdPipe);
                }
                writeLine(cmdPipe, "ERROR NULL");
            } else if (strstr(response, "ERROR ") == response) {
                if (debugMode) {
                    printf("%d: Sending error response: %s\n", cmdPipe, response);
                }
                writeLine(cmdPipe, response);
            } else {
                if (debugMode) {
                    printf("%d: Sending response: %s\n", cmdPipe, response);
                }
                
                int numLines = 1;
                
                for (int i = 0; i < strlen(response); i++) {
                    if (response[i] == '\n') {
                        numLines++;
                    }
                }
                
                char responseHeader[12];
                sprintf(responseHeader, "RESPONSE %d", numLines);
                
                writeLine(cmdPipe, responseHeader);
                writeLine(cmdPipe, response);
            }
            
            free(request);
        }
    }
    
    if (debugMode) {
        printf("%d: Client disconnected from command pipe\n", cmdPipe);
    }
    
    FlushFileBuffers(cmdPipe);
    DisconnectNamedPipe(cmdPipe);
    CloseHandle(cmdPipe);
    
    return 0;
}

char* handleRequest(HANDLE cmdPipe, char* request) {
    if (strstart(request, "COMMAND ")) {
        char command[strlen(request) - strlen("COMMAND ") + 1];
        strsub(request, command, 8, strlen(request));
        return handleRequestCommand(cmdPipe, command);
    } else if (strstart(request, "DATAREQ ")) {
        char dataRequest[strlen(request) - strlen("DATAREQ ") + 1];
        strsub(request, dataRequest, 8, strlen(request));
        return handleRequestData(cmdPipe, dataRequest);
    }
    
    return "ERROR UNKNOWN";
}

char* handleRequestCommand(HANDLE cmdPipe, char* command) {
    if (debugMode) {
        printf("%d: Request is a command: \"%s\"\n", cmdPipe, command);
    }
    
    if (strstart(command, "exec ") && strlen(command) > 5) {
        char execArgs[strlen(command) - strlen("exec ") + 1];
        strsub(command, execArgs, 5, strlen(command));
        return commandExec(cmdPipe, execArgs);
    } else if (strstart(command, "kill ") && strlen(command) > 5) {
        char killArgs[strlen(command) - strlen("kill ") + 1];
        strsub(command, killArgs, 5, strlen(command));
        return commandKill(cmdPipe, killArgs);
    } else if (strstart(command, "query ") && strlen(command) > 6) {
        char queryArgs[strlen(command) - strlen("query ") + 1];
        strsub(command, queryArgs, 6, strlen(command));
        return commandQuery(cmdPipe, queryArgs);
    }
    
    return "ERROR UNKNOWN";
}

char* handleRequestData(HANDLE cmdPipe, char* datareq) {
    if (debugMode) {
        printf("%d: Request is a data request: \"%s\"\n", cmdPipe, datareq);
    }
    
    // TODO: Handle data request
    
    return "ERROR UNKNOWN";
}

char* commandExec(HANDLE cmdPipe, char* execArgs) {
    if (debugMode) {
        printf("%d: Execute: \"%s\"\n", cmdPipe, execArgs);
    }
    
    // TODO: Execute process
    
    return "ERROR UNKNOWN";
}

char* commandKill(HANDLE cmdPipe, char* killArgs) {
    if (debugMode) {
        printf("%d: Kill: \"%s\"\n", cmdPipe, killArgs);
    }
    
    // TODO: Kill process
    
    return "ERROR UNKNOWN";
}

char* commandQuery(HANDLE cmdPipe, char* queryArgs) {
    if (debugMode) {
        printf("%d: Query: \"%s\"\n", cmdPipe, queryArgs);
    }
    
    // TODO: Query process
    
    return "ERROR UNKNOWN";
}
