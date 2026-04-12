#!/usr/bin/env python3
"""Pack WAV files into C header - copy files as-is."""
import os
import struct
import sys

TW_ASSETS_MAGIC = 0x54574153
TW_ASSETS_VERSION = 1
TW_ASSETS_MAX_NAME = 16
TW_ASSETS_HEADER_SIZE = 16

def pack_assets_raw(input_dir, output_path):
    """Pack WAV files by copying the raw file data."""
    wav_files = sorted([f for f in os.listdir(input_dir) if f.endswith('.wav')])
    if not wav_files:
        print(f"No WAV files in {input_dir}")
        return 1

    # Read all WAV files as-is
    entries = []
    data_section = b''
    data_offset = TW_ASSETS_HEADER_SIZE + 24 * len(wav_files)  # 24 = 16 name + 4 offset + 4 size
    
    for fname in wav_files:
        name = fname[:-4]
        if len(name) >= TW_ASSETS_MAX_NAME:
            name = name[:TW_ASSETS_MAX_NAME - 1]
        
        path = os.path.join(input_dir, fname)
        with open(path, 'rb') as f:
            wav_data = f.read()
        
        print(f"  {fname}: {len(wav_data)} bytes (raw)")
        
        entry = {
            'name': name.encode('ascii').ljust(TW_ASSETS_MAX_NAME, b'\x00'),
            'offset': data_offset,
            'size': len(wav_data),
            'data': wav_data,
        }
        entries.append(entry)
        data_offset += len(wav_data)
        data_section += wav_data
    
    # Build binary assets
    import io
    buf = io.BytesIO()
    buf.write(struct.pack('<I', TW_ASSETS_MAGIC))
    buf.write(struct.pack('<HH', TW_ASSETS_VERSION, len(wav_files)))
    buf.write(b'\x00' * 8)  # reserved
    for e in entries:
        buf.write(e['name'])
        buf.write(struct.pack('<I', e['offset']))
        buf.write(struct.pack('<I', e['size']))
    buf.write(data_section)
    binary = buf.getvalue()
    
    # Write C header
    with open(output_path, 'w') as f:
        f.write("/* Auto-generated - do not edit */\n")
        f.write("#ifndef SOUNDS_ASSETS_H\n")
        f.write("#define SOUNDS_ASSETS_H\n\n")
        f.write(f"static const uint8_t sounds_assets[] = {{\n")
        for i, b in enumerate(binary):
            if i % 16 == 0:
                f.write("    ")
            f.write(f"0x{b:02x},")
            if i % 16 == 15:
                f.write("\n")
        if len(binary) % 16 != 0:
            f.write("\n")
        f.write("};\n\n");
        f.write(f"static const uint32_t sounds_assets_len = {len(binary)};\n\n")
        f.write("#endif\n")
    
    print(f"Packed {len(wav_files)} -> {output_path} ({len(binary)} bytes)")
    return 0

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: pack_assets_raw.py <input_dir> <output.h>")
        sys.exit(1)
    sys.exit(pack_assets_raw(sys.argv[1], sys.argv[2]))