#!/usr/bin/env python3
import subprocess
import sys
import struct

def gen_table(kernel_elf, output_bin):
    try:
        result = subprocess.run(['nm', '-g', '-p', kernel_elf], capture_output=True, text=True, check=True)
    except Exception as e:
        print(f"error running nm: {e}")
        sys.exit(1)

    symbols = []
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) < 3: continue
        address, sym_type, name = parts[0], parts[1], parts[2]
        
        if sym_type.upper() != 'T': continue
        if "irq_handler_" in name: continue
        
        symbols.append((name, int(address, 16)))

    symbols.sort(key=lambda x: x[0])
    symbol_count = len(symbols)

    string_pool = b""
    sym_offsets = []
    
    for name, _ in symbols:
        sym_offsets.append(len(string_pool))
        string_pool += name.encode('utf-8') + b'\x00'

    with open(output_bin, 'wb') as f:
        f.write(struct.pack('<Q', symbol_count))

        for i, (name, addr) in enumerate(symbols):
            string_offset = sym_offsets[i]
            f.write(struct.pack('<QI', addr, string_offset))

        f.write(string_pool)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(1)

    gen_table(sys.argv[1], sys.argv[2])
