#!/usr/bin/env python3
"""Asset packer for Typewrite OS - creates .assets file from WAV files."""

import struct
import os
import sys

TW_ASSETS_MAGIC = 0x54574153  # "TWAS"
TW_ASSETS_VERSION = 1
TW_ASSETS_MAX_NAME = 16
TW_ASSETS_HEADER_SIZE = 16
TW_ASSETS_ENTRY_SIZE = 24  # 16 name + 4 offset + 4 size


def read_wav(path):
    """Read WAV file and return (sample_rate, channels, bits_per_sample, data)."""
    with open(path, 'rb') as f:
        if f.read(4) != b'RIFF':
            raise ValueError(f"Not a WAV: {path}")
        f.read(4)  # file size
        if f.read(4) != b'WAVE':
            raise ValueError(f"Not WAV format: {path}")
        
        data = b''
        sample_rate = 44100
        channels = 1
        bits = 16
        byte_rate = 0
        block_align = 0
        
        while True:
            chunk = f.read(8)
            if not chunk:
                break
            cid, size = struct.unpack('<4sI', chunk)
            chunk_data = f.read(size)
            
            if cid == b'fmt ':
                # fmt chunk: 16, 18, or 20 bytes
                # Bytes: 0-1 format, 2-3 channels, 4-7 sample rate, 8-11 byte rate, 12-13 block align, 14-15 bits
                if size >= 2:
                    audio_fmt = struct.unpack('<H', chunk_data[:2])[0]
                if size >= 4:
                    channels = struct.unpack('<H', chunk_data[2:4])[0]
                if size >= 8:
                    sample_rate = struct.unpack('<I', chunk_data[4:8])[0]
                    byte_rate = struct.unpack('<I', chunk_data[8:12])[0]
                    block_align = struct.unpack('<H', chunk_data[12:14])[0]
                # Bits per sample: reliable when size >= 18, but present at offset 14 for size >= 16
                if size == 16:
                    # For PCM (format 1) with 16-byte fmt, bits is at offset 14
                    bits = struct.unpack('<H', chunk_data[14:16])[0]
                elif size >= 18:
                    bits = struct.unpack('<H', chunk_data[14:16])[0]
                else:
                    bits = 16  # default
            elif cid == b'data':
                data = chunk_data
        
        if not data:
            raise ValueError(f"No data chunk: {path}")
        
        # Validate values
        if sample_rate == 0: sample_rate = 44100
        if channels == 0: channels = 1
        if bits == 0: bits = 16
        if byte_rate == 0: byte_rate = sample_rate * channels * bits // 8
        if block_align == 0: block_align = channels * bits // 8
        
        print(f"  {path}: rate={sample_rate} ch={channels} bits={bits} block={block_align} data={len(data)}")
        
        # Build proper WAV header
        wav_header = b'RIFF'
        data_size = len(data)  # This is the data chunk size from original WAV
        wav_size = 36 + data_size
        wav_header += struct.pack('<I', wav_size)
        wav_header += b'WAVE'
        wav_header += b'fmt '
        # byte_rate = sample_rate * channels * bits_per_sample / 8
        byte_rate = sample_rate * channels * bits // 8
        # block_align = channels * bits_per_sample / 8
        block_align = channels * bits // 8
        wav_header += struct.pack('<IHHIIHH', 16, 1, channels, sample_rate, 
                                   byte_rate, block_align, bits)
        wav_header += b'data'
        wav_header += struct.pack('<I', data_size)
        
        # Full WAV file = header + data (no padding needed for SDL)
        full_wav = wav_header + data
        
        return sample_rate, channels, bits, full_wav


def pad32(data):
    """Pad to 4-byte boundary."""
    rem = len(data) % 4
    if rem:
        return data + b'\x00' * (4 - rem)
    return data


def crc32(data):
    """Simple CRC32."""
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xEDB88320 if crc & 1 else crc >> 1
    return crc ^ 0xFFFFFFFF


def pack_assets(input_dir, output_path):
    """Pack WAV files into .assets format."""
    
    wav_files = sorted([f for f in os.listdir(input_dir) if f.endswith('.wav')])
    if not wav_files:
        print(f"No WAV files found in {input_dir}")
        return 1
    
    entries = []
    data_section = b''
    data_offset = TW_ASSETS_HEADER_SIZE + TW_ASSETS_ENTRY_SIZE * len(wav_files)
    
    for fname in wav_files:
        name = fname[:-4]  # remove .wav
        if len(name) >= TW_ASSETS_MAX_NAME:
            name = name[:TW_ASSETS_MAX_NAME - 1]
        
        path = os.path.join(input_dir, fname)
        _, _, _, wav_data = read_wav(path)
        
        # Full WAV already has proper header + data, no padding needed
        entry = {
            'name': name.encode('ascii').ljust(TW_ASSETS_MAX_NAME, b'\x00'),
            'offset': data_offset,
            'size': len(wav_data),
            'data': wav_data,
            'checksum': crc32(wav_data),
        }
        entries.append(entry)
        data_offset += len(wav_data)
        data_section += wav_data
    
    # Write .assets file
    with open(output_path, 'wb') as f:
        # Header (little endian)
        f.write(struct.pack('<I', TW_ASSETS_MAGIC))
        f.write(struct.pack('<H', TW_ASSETS_VERSION))
        f.write(struct.pack('<H', len(wav_files)))
        f.write(b'\x00' * 8)  # reserved
        
        # Entries
        for e in entries:
            f.write(e['name'])
            f.write(struct.pack('<I', e['offset']))
            f.write(struct.pack('<I', e['size']))
        
        # Data
        f.write(data_section)
    
    print(f"Packed {len(wav_files)} sounds -> {output_path} ({os.path.getsize(output_path)} bytes)")
    return 0


def pack_assets_as_c_header(input_dir, output_path):
    """Pack WAV files into C header format."""
    
    wav_files = sorted([f for f in os.listdir(input_dir) if f.endswith('.wav')])
    if not wav_files:
        print(f"No WAV files found in {input_dir}")
        return 1
    
    entries = []
    data_section = b''
    data_offset = TW_ASSETS_HEADER_SIZE + TW_ASSETS_ENTRY_SIZE * len(wav_files)
    
    for fname in wav_files:
        name = fname[:-4]
        if len(name) >= TW_ASSETS_MAX_NAME:
            name = name[:TW_ASSETS_MAX_NAME - 1]
        
        path = os.path.join(input_dir, fname)
        _, _, _, wav_data = read_wav(path)
        
        entry = {
            'name': name.encode('ascii').ljust(TW_ASSETS_MAX_NAME, b'\x00'),
            'offset': data_offset,
            'size': len(wav_data),
            'data': wav_data,
        }
        entries.append(entry)
        data_offset += len(wav_data)
        data_section += wav_data
    
    # Build binary assets file in memory
    import io
    buf = io.BytesIO()
    buf.write(struct.pack('<I', TW_ASSETS_MAGIC))
    buf.write(struct.pack('<HH', TW_ASSETS_VERSION, len(wav_files)))
    buf.write(b'\x00' * 8)
    for e in entries:
        buf.write(e['name'])
        buf.write(struct.pack('<I', e['offset']))
        buf.write(struct.pack('<I', e['size']))
    buf.write(data_section)
    binary_data = buf.getvalue()
    
    # Write as C header
    with open(output_path, 'w') as f:
        f.write("/* Auto-generated by pack_assets.py - do not edit */\n")
        f.write("#ifndef SOUNDS_ASSETS_H\n")
        f.write("#define SOUNDS_ASSETS_H\n\n")
        f.write(f"static const uint8_t sounds_assets[] = {{\n")
        for i, b in enumerate(binary_data):
            if i % 16 == 0:
                f.write("    ")
            f.write(f"0x{b:02x},")
            if i % 16 == 15:
                f.write("\n")
        if len(binary_data) % 16 != 0:
            f.write("\n")
        f.write("};\n\n")
        f.write(f"static const uint32_t sounds_assets_len = {len(binary_data)};\n\n")
        f.write("#endif /* SOUNDS_ASSETS_H */\n")
    
    print(f"Packed {len(wav_files)} sounds -> {output_path} ({len(binary_data)} bytes)")
    return 0


def list_assets(path):
    """List contents of .assets file."""
    with open(path, 'rb') as f:
        header = f.read(16)
        magic, version, count = struct.unpack('<IHH', header[:8])
        
        print(f"Magic: {'TWAS' if magic == TW_ASSETS_MAGIC else hex(magic)}")
        print(f"Version: {version}")
        print(f"Entries: {count}")
        print()
        
        for i in range(count):
            entry_data = f.read(24)
            name, offset, size = struct.unpack('<16sII', entry_data)
            name = name.rstrip(b'\x00').decode('ascii')
            print(f"  {name:20s} offset={offset:6d} size={size:6d}")


def extract_assets(path, output_dir):
    """Extract sounds from .assets file."""
    os.makedirs(output_dir, exist_ok=True)
    
    with open(path, 'rb') as f:
        header = f.read(16)
        magic, version, count = struct.unpack('<IHH', header[:8])
        
        entries = []
        for i in range(count):
            entry_data = f.read(24)
            name, offset, size = struct.unpack('<16sII', entry_data)
            name = name.rstrip(b'\x00').decode('ascii')
            entries.append((name, offset, size))
        
        for name, offset, size in entries:
            f.seek(offset)
            data = f.read(size)
            
            out_path = os.path.join(output_dir, f"{name}.wav")
            with open(out_path, 'wb') as out:
                out.write(b'RIFF')
                out.write(struct.pack('<I', 36 + size))
                out.write(b'WAVE')
                out.write(b'fmt ')
                out.write(struct.pack('<IHHIIHH', 16, 1, 1, 44100, 88200, 2, 16))
                out.write(b'data')
                out.write(struct.pack('<I', size))
                out.write(data)
            print(f"Extracted: {name}.wav")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage:")
        print("  pack_assets.py pack <input_dir> <output.assets>")
        print("  pack_assets.py header <input_dir> <output.h>")
        print("  pack_assets.py list <file.assets>")
        print("  pack_assets.py extract <file.assets> <output_dir>")
        sys.exit(1)
    
    cmd = sys.argv[1]
    
    if cmd == "pack":
        if len(sys.argv) != 4:
            print("pack_assets.py pack <input_dir> <output.assets>")
            sys.exit(1)
        sys.exit(pack_assets(sys.argv[2], sys.argv[3]))
    elif cmd == "header":
        if len(sys.argv) != 4:
            print("pack_assets.py header <input_dir> <output.h>")
            sys.exit(1)
        sys.exit(pack_assets_as_c_header(sys.argv[2], sys.argv[3]))
    elif cmd == "list":
        if len(sys.argv) != 3:
            print("pack_assets.py list <file.assets>")
            sys.exit(1)
        list_assets(sys.argv[2])
    elif cmd == "extract":
        if len(sys.argv) != 4:
            print("pack_assets.py extract <file.assets> <output_dir>")
            sys.exit(1)
        sys.exit(extract_assets(sys.argv[2], sys.argv[3]))
    else:
        print(f"Unknown command: {cmd}")
        sys.exit(1)
