"""Independent check of the DOS UI's single-entry metadata snapshot."""
import pathlib
import struct
import sys
import zlib

data = pathlib.Path(sys.argv[1]).read_bytes()
assert data[:8] == b'OTCAT1\r\n'
version, header, count, reserved, length, crc, reserved32, hcrc = struct.unpack_from('<HHHHIIII', data, 8)
large = len(sys.argv) > 2 and sys.argv[2] == "large"
assert (version, header, count, reserved, reserved32) == (1, 32, 256 if large else 1, 0, 0)
assert len(data) == 32 + length
assert zlib.crc32(data[:28]) == hcrc and zlib.crc32(data[32:]) == crc
assert data[88:120].split(b'\0')[0] == (b'LARGE' if large else b'TEST')
offset = 120
for i in range(count):
    size, reserved, session, obj = struct.unpack_from('<HHII', data, offset)
    assert not reserved and session and obj
    assert struct.unpack_from('<I', data, offset + 20)[0] == 35000 + (i if large else 0)
    expected = b'ABCDEFGH/' * 26 + f'F{i:03d}.BIN'.encode() if large else b'DATA/INPUT.BIN'
    assert data[offset + 32:offset + 12 + size] == expected
    offset += 12 + size
assert offset == len(data)
if large:
    assert len(data) > 65535
print('DOS catalogue independently checked: metadata only, exact size and CRC')
