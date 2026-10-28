"""Independent small offline metadata file; no tape seed or device query."""
import pathlib
import struct
import sys
import zlib

label = bytearray(88)
label[:8] = b'OPENTAPE'
struct.pack_into('<HHHHI', label, 8, 1, 88, 0, 0, 1)
label[56:60] = b'TEST'
path = b'DATA/INPUT.BIN'
metadata = struct.pack('<HHHHIHHI', 1, 1, 0x20, len(path), 35000, 0, 0, 0) + path
payload = label + struct.pack('<HHII', len(metadata), 0, 1, 1) + metadata
header = b'OTCAT1\r\n' + struct.pack('<HHHHIII', 1, 32, 1, 0, len(payload), zlib.crc32(payload), 0)
header += struct.pack('<I', zlib.crc32(header))
pathlib.Path(sys.argv[1]).write_bytes(header + payload)
