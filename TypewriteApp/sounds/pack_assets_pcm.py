#!/usr/bin/env python3
"""Pack WAV files into C header - extract raw PCM data only."""
import os
import struct
import sys

TW_ASSETS_MAGIC = 0x54574153
TW_ASSETS_VERSION = 1
TW_ASSETS_MAX_NAME = 16
TW_ASSETS_HEADER_SIZE = 16

def read_pcm_data(input_dir):
    """Read WAV file and return raw PCM data."""
    wav_files = sorted([f for f in os.listdir(input_dir) if f.endswith('.wav')])
    if not wav_files:
        return []
    
    entries = []
    data_section = b''
    data_offset = TW_ASSETS_HEADER_SIZE + 24 * len(wav_files)  # 24 = 16 name + 4 offset + 4 size
    
    for fname in wav_files:
        name = fname[:-4]
        if len(name) >= TW_ASSETS_MAX_NAME:
            name = name[:TW_ASSETS_MAX_NAME - 1]
        
        path = os.path.join(input_dir, fname)
        with open(path, 'rb') as f:
            # Verify RIFF/WAVE
            if f.read(4) != b'RIFF': continue
            f.read(4)  # size
            if f.read(4) != b'WAVE': continue
            
            # Find data chunk
            data = b''
            while True:
                chunk = f.read(8)
                if not chunk: break
                cid, size = struct.unpack('<4sI', chunk)
                if cid == b'data':
                    data = f.read(size)
                    break
                else:
                    # Skip this chunk
                    if size % 2: size += 1
                    f.read(size)
        
        if not data:
            print(f"  {fname}: no data!")
            continue
        
        print(f"  {fname}: {len(data)} bytes PCM")
        entries.append({
            'name': name.encode('ascii').ljust(TW_ASSETS_MAX_NAME, b'\x00'),
            'offset': data_offset,
            'size': len(data),
            'data': data,
        })
        data_offset += len(data)
        data_section += data
    
    return entries

def pack_assets_pcm(input_dir, output_path):
    entries = read_pcm_data(input_dir)
    if not entries:
        print("No WAV files found")
        return 1
    
    # Build binary
    import io
    buf = io.BytesIO()
    buf.write(struct.pack('<I', TW_ASSETS_MAGIC))
    buf.write(struct.pack('<HH', TW_ASSETS_VERSION, len(entries)))
    buf.write(b'\x00' * 8)
    for e in entries:
        buf.write(e['name'])
        buf.write(struct.pack('<I', e['offset']))
        buf.write(struct.pack('<I', e['size']))
    for e in entries:
        buf.write(e['data'])
    binary = buf.getvalue()
    
    # Write C header
    with open(output_path, 'w') as f:
        f.write("/* Auto-generated - do not edit */\n")
        f.write("#ifndef SOUNDS_ASSETS_H\n")
        f.write("#define SOUNDS_ASSETS_H\n\n")
        f.write(f"static const uint8_t sounds_assets[] = {{\n")
        for i, b in enumerate(binary):
            if i % 16 == 0: f.write("    ")
            f.write(f"0x{b:02x},")
            if i % 16 == 15: f.write("\n")
        if len(binary) % 16 != 0: f.write("\n")
        f.write("};\n\n");
        f.write(f"static const uint32_t sounds_assets_len = {len(binary)};\n\n")
        f.write("#endif\n")
    
    print(f"Packed {len(entries)} -> {output_path} ({len(binary)} bytes)")
    return 0

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: pack_assets_pcm.py <input_dir> <output.h>")
        sys.exit(1)
    sys.exit(pack_assets_pcm(sys.argv[1], sys.argv[2]))