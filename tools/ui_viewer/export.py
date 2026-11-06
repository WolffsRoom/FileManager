#!/usr/bin/env python3
"""
export.py - prepares the data the UI viewer draws from.

Reads the compiled build/file_manager_plugin.rco (exactly what the console loads)
and the firmware's common_resource.rco, and writes to tools/ui_viewer/cache/:

    app.json     element tree of our RCO, ids resolved to names
    common.json  styles and texture metadata of the system resource
    tex/*.png    every texture of both, decoded (GIM, PNG)
    strings.json the string tables of our locales

The firmware files are Sony's: cache/ is local only and must not be committed.

    python tools/ui_viewer/export.py [--firmware <Vita3K pref>/vs0]
"""
import argparse
import json
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import rcoc  # noqa: E402

DEFAULT_FW = os.path.join(os.environ.get("APPDATA", ""), "Vita3K", "Vita3K", "vs0")

# ------------------------------------------------------------------ PNG / GIM


def write_png(path, w, h, rgba):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)
        raw += rgba[y * stride:(y + 1) * stride]
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b""))


BPP = {0: 16, 1: 16, 2: 16, 3: 32, 4: 4, 5: 8, 6: 16, 7: 32}


def px(data, fmt, i):
    if fmt == 3:
        return data[i * 4:i * 4 + 4]
    v = struct.unpack_from("<H", data, i * 2)[0]
    if fmt == 2:
        return bytes(((v & 0xF) * 17, ((v >> 4) & 0xF) * 17, ((v >> 8) & 0xF) * 17, ((v >> 12) & 0xF) * 17))
    if fmt == 1:
        return bytes((round((v & 0x1F) * 255 / 31), round(((v >> 5) & 0x1F) * 255 / 31),
                      round(((v >> 10) & 0x1F) * 255 / 31), 255 if v >> 15 else 0))
    return bytes((round((v & 0x1F) * 255 / 31), round(((v >> 5) & 0x3F) * 255 / 63),
                  round(((v >> 11) & 0x1F) * 255 / 31), 255))


def unswizzle(data, wb, h):
    out = bytearray(len(data))
    src = 0
    for by in range(h // 8):
        for bx in range(wb // 16):
            for y in range(8):
                d = (by * 8 + y) * wb + bx * 16
                out[d:d + 16] = data[src:src + 16]
                src += 16
    return bytes(out)


def decode_gim(buf):
    image = palette = None

    def block(off, end):
        hs, _, fmt, sw, w, h = struct.unpack_from("<HHHHHH", buf, off)
        return {"fmt": fmt, "sw": bool(sw), "w": w, "h": h, "data": buf[off + hs:end]}

    def walk(start, end):
        nonlocal image, palette
        off = start
        while off + 16 <= end:
            ctype, _, size, nxt, doff = struct.unpack_from("<HHIII", buf, off)
            if size == 0:
                break
            if ctype in (2, 3):
                walk(off + 16, off + size)
            elif ctype == 4 and image is None:
                image = block(off + doff, off + size)
            elif ctype == 5 and palette is None:
                palette = block(off + doff, off + size)
            off += nxt if 0 < nxt < size else size

    walk(0x10, len(buf))
    fmt, w, h, data = image["fmt"], image["w"], image["h"], image["data"]
    if image["sw"]:
        rb = ((w * BPP[fmt]) // 8 + 15) & ~15
        hh = (h + 7) // 8 * 8
        data = unswizzle(data.ljust(rb * hh, b"\0"), rb, hh)
    out = bytearray()
    if fmt in (4, 5):
        pal = [px(palette["data"], palette["fmt"], i) for i in range(len(palette["data"]) // (BPP[palette["fmt"]] // 8))]
        rb = ((w if fmt == 5 else (w + 1) // 2) + 15) & ~15
        for y in range(h):
            row = data[y * rb:y * rb + rb]
            for x in range(w):
                i = row[x] if fmt == 5 else (row[x // 2] >> (4 * (x & 1))) & 0xF
                out += pal[i] if i < len(pal) else b"\0\0\0\0"
    else:
        bpp = BPP[fmt] // 8
        rb = (w * bpp + 15) & ~15
        for y in range(h):
            row = data[y * rb:y * rb + w * bpp]
            for x in range(w):
                out += px(row, fmt, x)
    return w, h, bytes(out)

# ------------------------------------------------------------------ RCO to JSON


def tree(r, names, texdir, prefix):
    hashes = r.s["hashes"]
    idh = r.s["idhash"]

    def nm(h):
        return names.get(h, "#%08x" % h)

    def value(el, ty, a, b, attrs):
        if ty == rcoc.T_INT:
            return struct.unpack("<i", struct.pack("<I", a))[0]
        if ty == rcoc.T_FLOAT:
            return round(struct.unpack("<f", struct.pack("<I", a))[0], 4)
        if ty == rcoc.T_STR:
            return r.string(a)
        if ty == rcoc.T_WSTR:
            return r.wstring(a)
        if ty == rcoc.T_INTS:
            return list(struct.unpack_from("<%di" % b, r.s["ints"], a * 4))
        if ty == rcoc.T_FLOATS:
            return [round(v, 4) for v in struct.unpack_from("<%df" % b, r.s["floats"], a * 4)]
        if ty == rcoc.T_HASH:
            return nm(struct.unpack_from("<I", hashes, a * 4)[0])
        if ty in (rcoc.T_IDHASH_SELF, rcoc.T_IDHASH_REF):
            return nm(struct.unpack_from("<I", idh, a + 4)[0])
        if ty in (rcoc.T_IDSTR_SELF, rcoc.T_IDSTR_REF):
            return r.idstr(a)
        if ty == rcoc.T_FILE:
            return {"file": a, "size": b}
        return None

    nodes = {}
    root = None
    for off, name, par, attrs in r.tags():
        d = {}
        for an, ty, a, b in attrs:
            d[an] = value(name, ty, a, b, attrs)
        node = {"tag": name, "a": d, "c": []}
        nodes[off] = node
        if par == rcoc.NONE:
            root = node
        else:
            nodes[par]["c"].append(node)

    # textures: decode now, keep only the png name
    for node in nodes.values():
        if node["tag"] != "texture" or not isinstance(node["a"].get("src"), dict):
            continue
        f = node["a"]["src"]
        blob = r.s["files"][f["file"]:f["file"] + f["size"]]
        if node["a"].get("compress") == "on" or blob[:2] == b"\x78\xda" or blob[:2] == b"\x78\x9c":
            try:
                blob = zlib.decompress(blob)
            except zlib.error:
                pass
        tid = node["a"].get("id", "tex")
        out = prefix + re.sub(r"[^\w]", "_", tid.replace("#", "h")) + ".png"
        try:
            if blob[:8] == b"\x89PNG\r\n\x1a\n":
                open(os.path.join(texdir, out), "wb").write(blob)
            elif blob[:4] == b"MIG.":
                w, h, rgba = decode_gim(blob)
                write_png(os.path.join(texdir, out), w, h, rgba)
            else:
                out = None
        except Exception as e:  # noqa: BLE001
            print("texture %s: %s" % (tid, e))
            out = None
        node["a"]["src"] = out
    return root


def names_from(*texts):
    out = {}
    for t in texts:
        for n in set(re.findall(r'"([A-Za-z_][\w]*)"', t)) | set(re.findall(r"#define (\w+)", t)):
            out[rcoc.paf_hash(n)] = n
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--firmware", default=DEFAULT_FW)
    ap.add_argument("--rco", default=os.path.join(ROOT, "build", "file_manager_plugin.rco"))
    args = ap.parse_args()

    cache = os.path.join(HERE, "cache")
    texdir = os.path.join(cache, "tex")
    os.makedirs(texdir, exist_ok=True)

    xml = open(os.path.join(ROOT, "resource", "file_manager_plugin.xml"), encoding="utf-8").read()
    extra = ""
    ns = os.path.join(ROOT, "..", "Files", "NetStream-main", "NetStream", "RES_RCO", "netstream_plugin.xml")
    if os.path.exists(ns):
        extra = open(ns, encoding="utf-8").read()
    # Element ids used inside system styles (plane_obj1, text_obj1, ...) are common names too.
    generic = " ".join('"%s_obj%d" "%sobj%d"' % (k, i, k, i) for k in ("plane", "label", "text", "image", "glow", "bg", "base", "corner", "highlight") for i in range(10))
    generic += ' "planeobj" "textobj" "glowobj" "label_obj" "bg_obj" "glow_obj" "image_obj" "corner_obj" "highlight_obj"'
    locales = ""
    for f in os.listdir(os.path.join(ROOT, "resource", "locale")):
        locales += open(os.path.join(ROOT, "resource", "locale", f), encoding="utf-8").read()
    names = names_from(xml, extra, generic, locales)

    app = rcoc.Rco(open(args.rco, "rb").read())
    app_tree = tree(app, names, texdir, "app_")

    # strings of every locale (locale files are RCS blobs inside the RCO)
    strings = {}
    for off, name, par, attrs in app.tags():
        if name != "locale":
            continue
        d = {an: (ty, a, b) for an, ty, a, b in attrs}
        lang = app.idstr(d["id"][1])
        f = d["src"]
        rcs = rcoc.Rco(app.s["files"][f[1]:f[1] + f[2]])
        table = {}
        for _, n2, _, at2 in rcs.tags():
            if n2 != "string":
                continue
            dd = {an: (ty, a, b) for an, ty, a, b in at2}
            ty, a, _ = dd["id"]
            if ty == rcoc.T_HASH:
                h = struct.unpack_from("<I", rcs.s["hashes"], a * 4)[0]
            else:
                h = struct.unpack_from("<I", rcs.s["idhash"], a + 4)[0]
            table[names.get(h, "#%08x" % h)] = rcs.wstring(dd["src"][1])
        strings[lang] = table

    common_path = os.path.join(args.firmware, "data", "external", "common", "common_resource.rco")
    common = rcoc.Rco(open(common_path, "rb").read())
    common_tree = tree(common, names, texdir, "common_")

    json.dump(app_tree, open(os.path.join(cache, "app.json"), "w"), ensure_ascii=False)
    json.dump(common_tree, open(os.path.join(cache, "common.json"), "w"), ensure_ascii=False)
    json.dump(strings, open(os.path.join(cache, "strings.json"), "w", encoding="utf-8"), ensure_ascii=False)
    print("cache:", cache, "textures:", len(os.listdir(texdir)))


if __name__ == "__main__":
    main()
