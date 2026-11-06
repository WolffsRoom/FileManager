import struct, zlib, sys

with open("resources/image/theme.png", "rb") as f:
    data = f.read()
print("Size:", len(data), "bytes")

if data[:8] == b"\x89PNG\r\n\x1a\n":
    print("Valid PNG signature")
else:
    print("INVALID PNG signature")
    sys.exit(1)

pos = 8
while pos < len(data):
    if pos + 8 > len(data):
        print(f"Truncated at {pos}")
        break
    length = struct.unpack(">I", data[pos:pos+4])[0]
    chunk_type = data[pos+4:pos+8]
    crc_start = pos + 8 + length
    if crc_start + 4 > len(data):
        print(f"Truncated chunk {chunk_type} at {pos}")
        break
    stored_crc = struct.unpack(">I", data[crc_start:crc_start+4])[0]
    computed_crc = zlib.crc32(data[pos+4:crc_start]) & 0xFFFFFFFF
    crc_ok = "OK" if stored_crc == computed_crc else "BAD"
    print(f"Chunk: {chunk_type} length={length} crc={crc_ok}")
    pos = crc_start + 4

print("Done")
