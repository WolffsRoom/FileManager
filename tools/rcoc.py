#!/usr/bin/env python3
"""
rcoc.py - open compiler and reader for PS Vita PAF resources (.rco / .rcs).

Sony's acdc.exe is not available, so this implements the format directly. It can
be verified against any XML/RCO pair: `verify` recompiles the XML and compares
each section with the reference RCO.

Format (CXML, version 0x110):

    "RCOF"|"RCSF", u32 version, then 9 (offset, size) pairs, sections 16-aligned:
    0 tree     7-u32 tag (name, nattr, parent, prev, next, first, last) + attrs
               of 4 u32 (name, type, a, b)
    1 idstr    (u32 owner, zero-terminated string padded to 4)
    2 idhash   (u32 owner or -1, u32 hash)
    3 strings  zero-terminated, UTF-8
    4 wstrings zero-terminated, UTF-16LE
    5 hashes   u32
    6 ints     i32
    7 floats   f32
    8 files    raw blobs, 16-aligned

Hash of a name: first 4 bytes of SHA-1, big endian.

Usage:
    rcoc.py compile in.xml -o out.rco [-x names.h] [--schema tools/rco_schema.json]
    rcoc.py dump in.rco
    rcoc.py schema a.rco b.rco ... -o tools/rco_schema.json
    rcoc.py verify in.xml reference.rco
"""

import argparse
import hashlib
import json
import os
import re
import struct
import sys
import xml.etree.ElementTree as ET

NONE = 0xFFFFFFFF
T_INT, T_FLOAT, T_STR, T_WSTR, T_HASH, T_INTS, T_FLOATS, T_FILE = 1, 2, 3, 4, 5, 6, 7, 8
T_IDSTR_SELF, T_IDSTR_REF, T_IDHASH_SELF, T_IDHASH_REF = 9, 10, 11, 12
SECTIONS = ("tree", "idstr", "idhash", "strings", "wstrings", "hashes", "ints", "floats", "files")


def paf_hash(name):
    return int.from_bytes(hashlib.sha1(name.encode("utf-8")).digest()[:4], "big")


def to_float(v):
    return float(v.strip().rstrip("fF"))


def align(n, a=16):
    return (n + a - 1) & ~(a - 1)


# ---------------------------------------------------------------- reader

class Rco:
    def __init__(self, data):
        self.data = data
        self.magic = data[:4]
        self.version = struct.unpack_from("<I", data, 4)[0]
        self.sec = [struct.unpack_from("<II", data, 8 + i * 8) for i in range(9)]
        self.s = {n: data[o:o + z] for n, (o, z) in zip(SECTIONS, self.sec)}

    def string(self, off):
        t = self.s["strings"]
        return t[off:t.index(b"\0", off)].decode("utf-8", "replace")

    def wstring(self, off):
        """off is in UTF-16 units, as the format stores it."""
        t = self.s["wstrings"]
        off *= 2
        e = off
        while t[e:e + 2] != b"\0\0":
            e += 2
        return t[off:e].decode("utf-16-le", "replace")

    def idstr(self, off):
        t = self.s["idstr"]
        return t[off + 4:t.index(b"\0", off + 4)].decode()

    def tags(self):
        """Yields (offset, name, parent_offset, [(attr, type, a, b)]) in document order."""
        t = self.s["tree"]
        o = 0
        while o < len(t):
            nm, na, par, _, _, _, _ = struct.unpack_from("<7I", t, o)
            attrs = []
            for i in range(na):
                an, ty, a, b = struct.unpack_from("<4I", t, o + 28 + i * 16)
                attrs.append((self.string(an), ty, a, b))
            yield o, self.string(nm), par, attrs
            o += 28 + na * 16


def dump(path):
    r = Rco(open(path, "rb").read())
    print(r.magic, hex(r.version), dict(zip(SECTIONS, r.sec)))
    depth = {NONE: -1}
    for off, name, par, attrs in r.tags():
        depth[off] = depth.get(par, -1) + 1
        parts = []
        for an, ty, a, b in attrs:
            if ty == T_STR:
                v = repr(r.string(a))
            elif ty == T_WSTR:
                v = repr(r.wstring(a))
            elif ty == T_FLOAT:
                v = "%g" % struct.unpack("<f", struct.pack("<I", a))[0]
            elif ty == T_INT:
                v = str(struct.unpack("<i", struct.pack("<I", a))[0])
            elif ty == T_FLOATS:
                v = str(list(struct.unpack_from("<%df" % b, r.s["floats"], a * 4)))
            elif ty == T_INTS:
                v = str(list(struct.unpack_from("<%di" % b, r.s["ints"], a * 4)))
            elif ty == T_HASH:
                v = "#%08x" % struct.unpack_from("<I", r.s["hashes"], a * 4)[0]
            elif ty in (T_IDHASH_SELF, T_IDHASH_REF):
                v = "@%08x" % struct.unpack_from("<I", r.s["idhash"], a + 4)[0]
            elif ty in (T_IDSTR_SELF, T_IDSTR_REF):
                v = repr(r.idstr(a))
            elif ty == T_FILE:
                v = "file[%#x+%d]" % (a, b)
            else:
                v = "?%x,%x" % (a, b)
            parts.append("%s:%d=%s" % (an, ty, v))
        print("  " * depth[off] + name, " ".join(parts))


def learn_schema(paths, out):
    """Records attribute types per element; earlier files take priority when types differ."""
    schema = {}

    def walk(r):
        for _, name, _, attrs in r.tags():
            for an, ty, a, b in attrs:
                ts = schema.setdefault(name, {}).setdefault(an, [])
                if ty not in ts:
                    ts.append(ty)
                if name == "locale" and ty == T_FILE and b > 16:
                    blob = r.s["files"][a:a + b]
                    if blob[:4] == b"RCSF":
                        walk(Rco(blob))

    for p in paths:
        walk(Rco(open(p, "rb").read()))
    with open(out, "w", encoding="utf-8") as f:
        json.dump(schema, f, indent=0, sort_keys=True)
    print("schema: %d elements -> %s" % (len(schema), out))


# ---------------------------------------------------------------- compiler

class Compiler:
    def __init__(self, schema, base_dir, magic=b"RCOF"):
        self.schema = schema
        self.base = base_dir
        self.magic = magic
        self.tree = bytearray()
        self.idstr = bytearray()
        self.idhash = bytearray()
        self.idhash_map = {}
        self.strings = bytearray()
        self.string_map = {}
        self.wstrings = bytearray()
        self.wstring_map = {}
        self.hashes = []
        self.hash_map = {}
        self.ints = []
        self.floats = []
        self.files = bytearray()
        self.file_map = {}
        self.names = {}
        self.warnings = []

    # pools ------------------------------------------------------------
    def str_off(self, s):
        if s not in self.string_map:
            self.string_map[s] = len(self.strings)
            self.strings += s.encode("utf-8") + b"\0"
        return self.string_map[s]

    def wstr_off(self, s):
        if s not in self.wstring_map:
            self.wstring_map[s] = len(self.wstrings)
            self.wstrings += s.encode("utf-16-le") + b"\0\0"
        return self.wstring_map[s]

    def hash_idx(self, s):
        h = paf_hash(s)
        if h not in self.hash_map:
            self.hash_map[h] = len(self.hashes)
            self.hashes.append(h)
        return self.hash_map[h]

    def idh_off(self, s, owner=None):
        h = paf_hash(s)
        if h not in self.idhash_map:
            self.idhash_map[h] = len(self.idhash)
            self.idhash += struct.pack("<II", NONE, h)
        o = self.idhash_map[h]
        if owner is not None:
            struct.pack_into("<I", self.idhash, o, owner)
        return o

    @staticmethod
    def pool_find(pool, vals):
        n = len(vals)
        for i in range(len(pool) - n + 1):
            if pool[i:i + n] == vals:
                return i
        pool.extend(vals)
        return len(pool) - n

    def add_file(self, element, value):
        path = value.replace("\\", "/")
        full = os.path.join(self.base, path)
        if not os.path.exists(full):
            # Locale sources may be given as "<dir>/name.xml.rcs" or "name.rcs".
            b = os.path.basename(path)
            stem = b[:-4] if b.endswith(".rcs") else b
            found = None
            for c in (stem, stem if stem.endswith(".xml") else stem + ".xml"):
                for d in ("locale", "."):
                    t = os.path.join(self.base, d, c)
                    if os.path.exists(t):
                        found = t
                        break
                if found:
                    break
            if not found:
                raise SystemExit("rcoc: file not found: %s" % full)
            full = found
        key = os.path.normpath(full)
        if key in self.file_map:
            return self.file_map[key]
        if element == "locale" and full.endswith(".xml"):
            blob = compile_file(full, self.schema, b"RCSF", names=self.names)
        else:
            blob = open(full, "rb").read()
        while len(self.files) % 16:
            self.files.append(0)
        res = (len(self.files), len(blob))
        self.files += blob
        self.file_map[key] = res
        return res

    # types ------------------------------------------------------------
    def attr_type(self, el, name, value):
        ts = self.schema.get(el, {}).get(name)
        if ts is None:
            # Same attribute on another element of the same family.
            seen = set()
            for attrs in self.schema.values():
                if name in attrs:
                    seen.update(attrs[name])
            if not seen:
                raise SystemExit("rcoc: unknown attribute <%s %s=...>; add it to the schema" % (el, name))
            self.warnings.append("<%s %s> not in schema, inferred from other elements" % (el, name))
            ts = sorted(seen)
        multi = "," in value
        if T_FLOATS in ts or T_FLOAT in ts or T_INTS in ts or T_INT in ts:
            is_float = T_FLOATS in ts or T_FLOAT in ts
            if is_float and multi and T_FLOATS in ts:
                return T_FLOATS
            if not is_float and multi and T_INTS in ts:
                return T_INTS
            if is_float and T_FLOAT in ts and not multi:
                return T_FLOAT
            if not is_float and T_INT in ts and not multi:
                return T_INT
            return T_FLOATS if is_float else T_INTS
        return ts[0]

    def encode(self, el, name, value, tag_off):
        ty = self.attr_type(el, name, value)
        if ty == T_INT:
            return ty, int(value, 0) & NONE, 0
        if ty == T_FLOAT:
            return ty, struct.unpack("<I", struct.pack("<f", to_float(value)))[0], 0
        if ty == T_STR:
            return ty, self.str_off(value), len(value.encode("utf-8"))
        if ty == T_WSTR:
            # wide strings: offset counted in UTF-16 units, size in bytes
            return ty, self.wstr_off(value) // 2, len(value.encode("utf-16-le"))
        if ty == T_HASH:
            self.names[value] = paf_hash(value)
            return ty, self.hash_idx(value), 4
        if ty == T_INTS:
            vals = [int(v.strip(), 0) for v in value.split(",")]
            return ty, self.pool_find(self.ints, vals), len(vals)
        if ty == T_FLOATS:
            vals = [to_float(v) for v in value.split(",")]
            return ty, self.pool_find(self.floats, vals), len(vals)
        if ty == T_FILE:
            o, z = self.add_file(el, value)
            return ty, o, z
        if ty == T_IDSTR_SELF:
            o = len(self.idstr)
            enc = value.encode() + b"\0"
            self.idstr += struct.pack("<I", tag_off) + enc + b"\0" * (align(len(enc), 4) - len(enc))
            return ty, o, 0
        if ty == T_IDHASH_SELF:
            self.names[value] = paf_hash(value)
            return ty, self.idh_off(value, tag_off), 0
        if ty == T_IDHASH_REF:
            return ty, self.idh_off(value), 0
        raise SystemExit("rcoc: unsupported type %d for <%s %s>" % (ty, el, name))

    # tree -------------------------------------------------------------
    def emit(self, el, parent, prev):
        off = len(self.tree)
        attrs = list(el.attrib.items())
        name_off = self.str_off(el.tag)
        self.tree += b"\0" * (28 + 16 * len(attrs))
        recs = []
        for i, (an, val) in enumerate(attrs):
            an_off = self.str_off(an)
            ty, a, b = self.encode(el.tag, an, val, off)
            struct.pack_into("<4I", self.tree, off + 28 + i * 16, an_off, ty, a, b)
        first = last = NONE
        p = NONE
        for child in list(el):
            c = self.emit(child, off, p)
            if p != NONE:
                struct.pack_into("<I", self.tree, p + 16, c)
            else:
                first = c
            p = last = c
        struct.pack_into("<7I", self.tree, off, name_off, len(attrs), parent, prev, NONE, first, last)
        return off

    def build(self, root):
        self.emit(root, NONE, NONE)
        secs = [
            bytes(self.tree), bytes(self.idstr), bytes(self.idhash), bytes(self.strings), bytes(self.wstrings),
            struct.pack("<%dI" % len(self.hashes), *self.hashes),
            struct.pack("<%di" % len(self.ints), *self.ints),
            struct.pack("<%df" % len(self.floats), *self.floats),
            bytes(self.files),
        ]
        hdr = bytearray(self.magic + struct.pack("<I", 0x110))
        off = 8 + 9 * 8
        body = bytearray()
        for s in secs:
            off2 = align(off)
            body += b"\0" * (off2 - off)
            hdr += struct.pack("<II", off2, len(s))
            body += s
            off = off2 + len(s)
        return bytes(hdr + body)


def parse(path):
    return ET.parse(path).getroot()


def compile_file(path, schema, magic=b"RCOF", names=None):
    root = parse(path)
    c = Compiler(schema, os.path.dirname(os.path.abspath(path)), magic)
    blob = c.build(root)
    for w in sorted(set(c.warnings)):
        print("rcoc: warning:", w, file=sys.stderr)
    if names is not None:
        names.update(c.names)
    return blob


def write_header(path, names):
    with open(path, "w", newline="\n") as f:
        f.write("// Generated by tools/rcoc.py - do not edit.\n#pragma once\n\n")
        for n, h in sorted(names.items()):
            if re.match(r"^[A-Za-z_]\w*$", n):
                f.write("#define %s (0x%08xu)\n" % (n, h))


def verify(xml, ref, schema):
    a = compile_file(xml, schema)
    ra, rb = Rco(a), Rco(open(ref, "rb").read())
    ok = True
    for n in SECTIONS:
        x, y = ra.s[n], rb.s[n]
        same = x == y
        ok &= same
        msg = "OK" if same else "DIFF"
        if not same:
            i = next((i for i in range(min(len(x), len(y))) if x[i] != y[i]), min(len(x), len(y)))
            msg += " first at %#x (ours %d B, ref %d B)" % (i, len(x), len(y))
        print("%-9s %8d  %s" % (n, len(y), msg))
    print("IDENTICAL" if a == rb.data else "total: ours %d, ref %d" % (len(a), len(rb.data)))
    return ok


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    here = os.path.dirname(os.path.abspath(__file__))
    default_schema = os.path.join(here, "rco_schema.json")
    p = sub.add_parser("compile"); p.add_argument("xml"); p.add_argument("-o", required=True)
    p.add_argument("-x"); p.add_argument("--schema", default=default_schema)
    p = sub.add_parser("dump"); p.add_argument("rco")
    p = sub.add_parser("schema"); p.add_argument("rco", nargs="+"); p.add_argument("-o", required=True)
    p = sub.add_parser("verify"); p.add_argument("xml"); p.add_argument("rco"); p.add_argument("--schema", default=default_schema)
    args = ap.parse_args()
    if args.cmd == "dump":
        dump(args.rco)
    elif args.cmd == "schema":
        learn_schema(args.rco, args.o)
    else:
        schema = json.load(open(args.schema))
        if args.cmd == "verify":
            sys.exit(0 if verify(args.xml, args.rco, schema) else 1)
        names = {}
        blob = compile_file(args.xml, schema, names=names)
        with open(args.o, "wb") as f:
            f.write(blob)
        if args.x:
            write_header(args.x, names)
        print("rcoc: %s (%d B, %d names)" % (args.o, len(blob), len(names)))


if __name__ == "__main__":
    main()
