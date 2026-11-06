import struct
with open('/mnt/c/Users/wolff/Documents/SDKVita/VitaShell-master/media/Theme/theme.gif','rb') as f:
    d = f.read()
w = struct.unpack('<H', d[6:8])[0]
h = struct.unpack('<H', d[8:10])[0]
print(f'GIF: {w}x{h}, {len(d)} bytes')
pos = 10
flags = d[pos]
gct_size = 2 ** ((flags & 7) + 1)
pos += 1 + 1 + gct_size * 3
frames = 0
while pos < len(d):
    if d[pos] == 0x3B:
        break
    if d[pos] == 0x21:
        pos += 2
        while pos < len(d) and d[pos] != 0:
            pos += d[pos] + 1
        pos += 1
    elif d[pos] == 0x2C:
        frames += 1
        img_w = struct.unpack('<H', d[pos+5:pos+7])[0]
        img_h = struct.unpack('<H', d[pos+7:pos+9])[0]
        print(f'  Frame {frames}: {img_w}x{img_h}')
        pos += 10
        if d[pos] & 0x80:
            lct_size = 2 ** ((d[pos] & 7) + 1)
            pos += 1 + lct_size * 3
        else:
            pos += 1
        while pos < len(d) and d[pos] != 0:
            pos += d[pos] + 1
        pos += 1
    else:
        pos += 1
print(f'Total frames: {frames}')
mem_per_frame = w * h * 4
total_mem = mem_per_frame * frames
print(f'Memory per frame: {mem_per_frame} bytes ({mem_per_frame/1024:.1f} KB)')
print(f'Total frame memory: {total_mem} bytes ({total_mem/1024/1024:.1f} MB)')
