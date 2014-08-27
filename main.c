
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <process.h>
#include <tlhelp32.h>
#include <psapi.h>

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
    
    /* Acquire SeDebugPrivilege */
    
    HANDLE processToken;
    TOKEN_PRIVILEGES privileges;
    TOKEN_PRIVILEGES privilegesPrevious;
    DWORD privilegesPreviousSize;
    LUID luid;
    
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &processToken)) {
        if (GetLastError() == ERROR_NO_TOKEN) {
            if (ImpersonateSelf(SecurityImpersonation)) {
                OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &processToken);
            }
        }
    }
    
    if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = 0;
        
        AdjustTokenPrivileges(processToken, FALSE, &privileges, sizeof(TOKEN_PRIVILEGES), &privilegesPrevious, &privilegesPreviousSize);
        
        if (GetLastError() == ERROR_SUCCESS) {
            privilegesPrevious.PrivilegeCount = 1;
            privilegesPrevious.Privileges[0].Luid = luid;
            privilegesPrevious.Privileges[0].Attributes |= SE_PRIVILEGE_ENABLED;
            
            AdjustTokenPrivileges(processToken, FALSE, &privilegesPrevious, privilegesPreviousSize, NULL, NULL);
            
            if (GetLastError() != ERROR_SUCCESS && debugMode) {
                printf("Could not enable SeDebugPrivilege");
            }
        }
    }
    
    CloseHandle(processToken);
    
    /* Communication */
    
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
    
    CloseHandle(cmdPipe);
    
    if (debugMode) {
        printf("JEXESVC is exiting...\n");
    }
    
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
            free(response);
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
    
    /* Split and Store Arguments */
    
    char authString[194];
    char startingDirectory[MAX_PATH];
    char envString[128];
    char redirectFlagString[1];
    char command[MAX_PATH];
    
    char* authStringIndex = authString;
    char* startingDirectoryIndex = startingDirectory;
    char* envStringIndex = envString;
    char* redirectFlagStringIndex = redirectFlagString;
    char* commandIndex = command;
    
    int index = 0;
    int quote = 0;
    
    for (int i = 0; i < strlen(execArgs); i++) {
        char c = execArgs[i];
        
        if (c == '"') {
            quote = !quote;
        } else if (c == ' ' && !quote) {
            index++;
        } else {
            switch (index) {
            case 0:
                *(authStringIndex++) = c;
                break;
            case 1:
                *(startingDirectoryIndex++) = c;
                break;
            case 2:
                *(envStringIndex++) = c;
                break;
            case 3:
                *(redirectFlagStringIndex++) = c;
                break;
            case 4:
                *(commandIndex++) = c;
                break;
            }
        }
    }
    
    *authStringIndex = '\0';
    *startingDirectoryIndex = '\0';
    *envStringIndex = '\0';
    *redirectFlagStringIndex = '\0';
    *commandIndex = '\0';
    
    /* Process Creation */
    
    if (strcmp(authString, "null") == 0) {
        // TODO: Create process without authentication
    } else {
        // TODO: Create process with authentication
    }
    
    return "ERROR UNKNOWN";
}

char* commandKill(HANDLE cmdPipe, char* killArgs) {
    if (debugMode) {
        printf("%d: Kill: \"%s\"\n", cmdPipe, killArgs);
    }
    
    int success = 0;
    
    char* end;
    int processExitCode = (int) strtol(killArgs, &end, 10);
    int processId = (int) strtol(end, NULL, 10);
    
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    
    if (TerminateProcess(process, processExitCode)) {
        if (debugMode) {
            printf("%d: Successfully terminated process %d with exit code %d\n", cmdPipe, processId, processExitCode);
        }
        
        success = 1;
    }
    
    CloseHandle(process);
    
    if (success) {
        return "SUCCESS";
    } else {
        return "FAIL";
    }
}

char* commandQuery(HANDLE cmdPipe, char* queryArgs) {
    if (strcmp(queryArgs, "processes") == 0) {
        if (debugMode) {
            printf("%d: Querying processes\n", cmdPipe);
        }
        
        char* response = NULL;
        
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);
        
        BOOL result = Process32First(snapshot, &processEntry);
        while (result) {
            int processId = processEntry.th32ProcessID;
            char* processExecutableName = processEntry.szExeFile;
            
            char processFilePath[MAX_PATH];
            char processUserName[256];
            char processUserDomain[256];
            
            DWORD processUserNameSize = sizeof(processUserName);
            DWORD processUserDomainSize = sizeof(processUserDomain);
            
            HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processEntry.th32ProcessID);
            
            char processFilePathNT[MAX_PATH];
            if (GetProcessImageFileName(process, (LPSTR) &processFilePathNT, MAX_PATH)) {
                convertNTPathToWin32Path(processFilePathNT, processFilePath);
            } else {
                sprintf(processFilePath, "ERROR(%d)", GetLastError());
            }
            
            HANDLE processToken;
            if (OpenProcessToken(process, TOKEN_READ, &processToken)) {
                DWORD tokenUserSize = 0;
                GetTokenInformation(processToken, TokenUser, NULL, 0, &tokenUserSize);
                
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    PTOKEN_USER userToken = (PTOKEN_USER) malloc(tokenUserSize);
                    
                    if (GetTokenInformation(processToken, TokenUser, userToken, tokenUserSize, &tokenUserSize)) {
                        SID_NAME_USE use;
                        LookupAccountSid(NULL, userToken->User.Sid, processUserName, &processUserNameSize, processUserDomain, &processUserDomainSize, &use);
                    }
                    
                    free(userToken);
                }
                
                CloseHandle(processToken);
            } else {
                sprintf(processUserName, "ERROR(%d)", GetLastError());
            }
            
            CloseHandle(process);
            
            char responseEntry[512];
            char responseEntryFormat[] = "[pid=%d,name=\"%s\",path=\"%s\",domain=\"%s\",user=\"%s\"]\n";
            sprintf(responseEntry, responseEntryFormat, processId, processExecutableName, processFilePath, processUserDomain, processUserName);
            strcatd(&response, responseEntry);
            
            result = Process32Next(snapshot, &processEntry);
        }
        
        CloseHandle(snapshot);
        
        return response;
    } else if (strcmp(queryArgs, "windows") == 0) {
        if (debugMode) {
            printf("%d: Querying windows\n", cmdPipe);
        }
        
        char* response = NULL;
        EnumWindows(commandQueryEnumWindowsProc, (LPARAM) &response);
        return response;
    }
    
    return "ERROR UNKNOWN";
}

BOOL CALLBACK commandQueryEnumWindowsProc(HWND hwnd, LPARAM lParam) {
    /* Filter Windows */
    
    HWND test, above;
    
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    
    above = GetAncestor(hwnd, GA_ROOTOWNER);
    while (test != above) {
        above = test;
        test = GetLastActivePopup(above);
        if (IsWindowVisible(test)) {
            break;
        }
    }
    if (test != above) {
        return TRUE;
    }
    
    if (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
        return TRUE;
    }
    
    /* Handle Windows */
    
    char windowTitle[128];
    DWORD windowProcessId;
    
    GetWindowText(hwnd, windowTitle, sizeof(windowTitle));
    GetWindowThreadProcessId(hwnd, &windowProcessId);
    
    char responseEntry[512];
    char responseEntryFormat[] = "[title=\"%s\",pid=%d]\n";
    sprintf(responseEntry, responseEntryFormat, windowTitle, windowProcessId);
    strcatd((char**) lParam, responseEntry);
    
    return TRUE;
}
