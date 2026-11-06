import struct, zlib, sys

data = open('/mnt/c/Users/wolff/Documents/SDKVita/VitaShell-master/resources/image/theme.png', 'rb').read()
print('Size:', len(data))
print('Sig OK:', data[:8] == b'\x89PNG\r\n\x1a\n')
pos = 8
while pos < len(data):
    if pos + 8 > len(data):
        print(f'Truncated at {pos}')
        break
    l = struct.unpack('>I', data[pos:pos+4])[0]
    t = data[pos+4:pos+8].decode()
    if pos + 8 + l + 4 > len(data):
        print(f'Chunk {t}: truncated, need {l} data + 4 CRC')
        break
    c = struct.unpack('>I', data[pos+8+l:pos+12+l])[0]
    cc = zlib.crc32(data[pos+4:pos+8+l]) & 0xFFFFFFFF
    ok = 'OK' if c == cc else 'BAD'
    print(f'{t} len={l} stored_crc=0x{c:08X} computed_crc=0x{cc:08X} {ok}')
    pos += 12 + l
print('EOF at', pos)
