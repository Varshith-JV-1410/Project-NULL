# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0] - 2026-10-12

### Added
- **POSIX Self-Deletion** — Novel `FILE_DISPOSITION_POSIX_SEMANTICS` technique for Windows 11 24H2
- **Basic Injector** (`main.cpp`) — CreateRemoteThread injection + POSIX self-deletion
- **Emoji Injector** (`simple_emoji_injector.cpp`) — Full obfuscation pipeline:
  - Unicode emoji steganography (variation selectors U+FE00-U+FE0F)
  - AES-256-CBC encryption with SHA-256 key derivation
  - Split + XOR-encoded AES key chunks (4 × 64-bit)
  - Compile-time XOR string obfuscation
  - POSIX-based self-deletion
- **Build Scripts**:
  - `build_payload.py` — Basic shellcode-to-executable pipeline
  - `build_emoji_payload.py` — Full obfuscation build pipeline
- **Cross-platform support** — Builds on Kali Linux, Ubuntu, macOS, Windows (MSYS2/WSL)
- **Demo video** (`NullCon_Self_Del.mp4`) — from the Nullcon 2026 presentation
- **MIT License**

### Acknowledgments
- [tkyn](https://tkyn.dev/) — NTFS driver analysis
- [LloydLabs](https://github.com/LloydLabs/delete-self-poc) — Original self-deletion technique
- [Jonas Lykkegård](https://x.com/jonasLyk) — Self-deletion discovery

[1.0.0]: https://github.com/Varshith-JV-1410/Project-NULL/releases/tag/v1.0.0
