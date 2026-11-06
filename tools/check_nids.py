#!/usr/bin/env python3
"""
check_nids.py - proves that every ScePaf/CommonGuiDialog import of an application
exists in real firmware modules, without running anything on hardware.

    check_nids.py build/FileManager.velf <firmware>/vs0/sys/external/libpaf.suprx \
                  <firmware>/vs0/sys/external/common_gui_dialog.suprx

Reads the import table of the .velf and the export tables of decrypted SELF modules
(such as a Vita3K firmware install). Exits non-zero when a NID is missing.
"""
import struct
import sys
import zlib

CHECKED_PREFIXES = ("ScePaf", "SceCommonGuiDialog")


def load_self(path):
    """Returns the ELF program segments of a decrypted SELF as [(vaddr, bytes)], and the module info."""
    d = open(path, "rb").read()
    if d[:4] != b"SCE\0":
        raise SystemExit("%s: not a SELF" % path)
    elf_off, ph_off = struct.unpack_from("<QQ", d, 64)
    seg_info = struct.unpack_from("<Q", d, 88)[0]
    entry = struct.unpack_from("<I", d, elf_off + 24)[0]
    phnum = struct.unpack_from("<H", d, elf_off + 44)[0]
    segs = []
    for i in range(phnum):
        ptype, _, vaddr, _, filesz, memsz, _, _ = struct.unpack_from("<8I", d, ph_off + i * 32)
        off, length, comp, enc = struct.unpack_from("<QQQQ", d, seg_info + i * 32)
        if enc == 1:
            raise SystemExit("%s: encrypted, need a decrypted module" % path)
        if ptype != 1:
            segs.append((None, b""))
            continue
        blob = d[off:off + length]
        if comp == 2:
            blob = zlib.decompress(blob)
        segs.append((vaddr, blob))
    # Bits 30-31 of e_entry hold the segment index, the rest is the offset of the module info.
    seg_index = entry >> 30
    info_off = entry & 0x3FFFFFFF
    return segs, seg_index, info_off


def exports_of(path):
    segs, si, off = load_self(path)
    base, blob = segs[si]
    # module info: attr(2) ver(2) name(27) type(1) gp(4) ent_top(4) ent_end(4) stub_top(4) stub_end(4)
    ent_top, ent_end = struct.unpack_from("<II", blob, off + 0x24)
    libs = {}
    q = ent_top
    while q < ent_end:
        size = struct.unpack_from("<H", blob, q)[0]
        nfunc, nvar = struct.unpack_from("<HH", blob, q + 6)
        libnid = struct.unpack_from("<I", blob, q + 0x14)[0]
        nid_tab = struct.unpack_from("<I", blob, q + 0x1C)[0]
        name_ptr = struct.unpack_from("<I", blob, q + 0x18)[0]
        name = ""
        if name_ptr:
            o = name_ptr - base
            if 0 <= o < len(blob):
                name = blob[o:blob.index(b"\0", o)].decode("latin1")
        nids = set()
        if nid_tab:
            o = nid_tab - base
            for i in range(nfunc + nvar):
                nids.add(struct.unpack_from("<I", blob, o + i * 4)[0])
        libs[libnid] = (name, nids)
        q += size
    return libs


def imports_of(velf):
    d = open(velf, "rb").read()
    phoff = struct.unpack_from("<I", d, 28)[0]
    phnum = struct.unpack_from("<H", d, 44)[0]
    loads = []
    for i in range(phnum):
        ptype, off, vaddr, _, filesz, _, _, _ = struct.unpack_from("<8I", d, phoff + i * 32)
        if ptype == 1:
            loads.append((vaddr, off, filesz))

    def to_off(va):
        for vaddr, off, filesz in loads:
            if vaddr <= va < vaddr + filesz:
                return off + va - vaddr
        return None

    shoff = struct.unpack_from("<I", d, 32)[0]
    shnum, shstr = struct.unpack_from("<HH", d, 48)
    strtab = struct.unpack_from("<I", d, shoff + shstr * 40 + 16)[0]
    stubs = None
    for i in range(shnum):
        sh = struct.unpack_from("<10I", d, shoff + i * 40)
        name = d[strtab + sh[0]:d.index(b"\0", strtab + sh[0])]
        if name == b".sceLib.stubs":
            stubs = (sh[4], sh[5])
    if not stubs:
        raise SystemExit("%s: no .sceLib.stubs section" % velf)

    out = []
    q, end = stubs[0], stubs[0] + stubs[1]
    while q < end:
        size = struct.unpack_from("<H", d, q)[0]
        nfunc, nvar = struct.unpack_from("<HH", d, q + 6)
        libnid, name_ptr = struct.unpack_from("<II", d, q + 0x10)
        func_nids = struct.unpack_from("<I", d, q + 0x1C)[0]
        var_nids = struct.unpack_from("<I", d, q + 0x24)[0]
        o = to_off(name_ptr)
        name = d[o:d.index(b"\0", o)].decode("latin1") if o else "?"
        nids = []
        for table, n in ((func_nids, nfunc), (var_nids, nvar)):
            if table and n:
                o = to_off(table)
                nids += list(struct.unpack_from("<%dI" % n, d, o))
        out.append((name, libnid, nids))
        q += size
    return out


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    exports = {}
    for module in sys.argv[2:]:
        exports.update(exports_of(module))

    missing = 0
    total = 0
    for name, libnid, nids in imports_of(sys.argv[1]):
        if not name.startswith(CHECKED_PREFIXES):
            continue
        if libnid not in exports:
            print("MISSING LIBRARY %s 0x%08X" % (name, libnid))
            missing += len(nids)
            total += len(nids)
            continue
        have = exports[libnid][1]
        bad = [n for n in nids if n not in have]
        total += len(nids)
        missing += len(bad)
        print("%-22s 0x%08X  %4d imports, %d missing" % (name, libnid, len(nids), len(bad)))
        for n in bad:
            print("    0x%08X" % n)
    print("%d imports checked, %d missing" % (total, missing))
    sys.exit(1 if missing else 0)


if __name__ == "__main__":
    main()
