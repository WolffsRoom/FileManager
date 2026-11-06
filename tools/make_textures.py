#!/usr/bin/env python3
"""Draws the app's own textures (device icons, neutral theme background) with the standard library only."""
import math
import os
import struct
import zlib

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "resource", "texture")
SS = 4
WHITE = (235, 240, 247)
DARK = (120, 140, 165)
BLUE = (70, 130, 200)


def rrect(x, y, l, t, r, b, rad):
    if x < l or x > r or y < t or y > b:
        return False
    dx = max(l + rad - x, 0, x - (r - rad))
    dy = max(t + rad - y, 0, y - (b - rad))
    return dx * dx + dy * dy <= rad * rad


def card(x, y):
    if not rrect(x, y, 18, 10, 62, 70, 5):
        return None
    if x + (24 - y) > 62 and y < 24 and x > 48:
        return None
    if 58 <= y <= 66 and 23 <= x <= 58 and ((x - 23) % 7) < 4:
        return DARK
    if 24 <= x <= 50 and 18 <= y <= 42:
        return BLUE
    return WHITE


def usb(x, y):
    if rrect(x, y, 30, 8, 50, 30, 2):
        return DARK if (34 <= x <= 39 or 42 <= x <= 47) and 14 <= y <= 20 else (200, 208, 220)
    if rrect(x, y, 22, 28, 58, 72, 6):
        return BLUE if rrect(x, y, 34, 44, 46, 58, 2) else WHITE
    return None


def internal(x, y):
    if rrect(x, y, 14, 18, 66, 62, 5):
        if rrect(x, y, 20, 24, 60, 40, 2):
            return BLUE
        if 48 <= y <= 54 and 22 <= x <= 58 and ((x - 22) % 8) < 5:
            return DARK
        return WHITE
    return None


def system(x, y):
    d = math.hypot(x - 40, y - 40)
    a = math.atan2(y - 40, x - 40)
    outer = 28 + (4 if math.cos(a * 8) > 0 else 0)
    if d <= 11:
        return None
    if d <= outer:
        return WHITE
    return None


def game(x, y):
    if rrect(x, y, 16, 16, 64, 64, 6):
        if rrect(x, y, 23, 23, 57, 46, 2):
            return BLUE
        if 52 <= y <= 60 and 24 <= x <= 56 and ((x - 24) % 6) < 3:
            return DARK
        return WHITE
    return None


def network(x, y):
    d = math.hypot(x - 40, y - 58)
    a = math.atan2(y - 58, x - 40)
    if -2.4 < a < -0.74:
        for r in (14, 26, 38):
            if r - 4 <= d <= r + 1:
                return WHITE
    if d <= 5:
        return BLUE
    return None


def checkbox_off(x, y):
    if rrect(x, y, 8, 8, 72, 72, 10) and not rrect(x, y, 14, 14, 66, 66, 6):
        return WHITE
    return None


def checkbox_on(x, y):
    if not rrect(x, y, 8, 8, 72, 72, 10):
        return None
    # check mark: two strokes
    def seg(px, py, ax, ay, bx, by, w):
        dx, dy = bx - ax, by - ay
        t = max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
        return math.hypot(px - (ax + t * dx), py - (ay + t * dy)) <= w
    if seg(x, y, 22, 41, 35, 54, 5) or seg(x, y, 35, 54, 59, 27, 5):
        return (30, 70, 140)
    return WHITE


def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))


def write_png(name, w, h, color_type, raw):
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, color_type, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    with open(os.path.join(OUT, name), "wb") as f:
        f.write(png)


def icon(name, fn):
    raw = bytearray()
    for py in range(80):
        raw.append(0)
        for px in range(80):
            cov = 0
            c = [0, 0, 0]
            for j in range(SS):
                for i in range(SS):
                    v = fn(px + (i + .5) / SS, py + (j + .5) / SS)
                    if v:
                        cov += 1
                        for k in range(3):
                            c[k] += v[k]
            if cov:
                raw += bytes((c[0] // cov, c[1] // cov, c[2] // cov, 255 * cov // (SS * SS)))
            else:
                raw += b"\0\0\0\0"
    write_png(name, 80, 80, 6, raw)


def neutral():
    # Light center and darker edges; themes multiply a color over it.
    w, h = 240, 136
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            nx = (x - w / 2) / (w / 2)
            ny = (y - h * 0.35) / h
            v = 1.0 - 0.45 * min(1.0, math.sqrt(nx * nx * 0.7 + ny * ny * 2.2))
            g = int(255 * v)
            raw += bytes((g, g, g, 255))
    write_png("tex_bg_neutral.png", w, h, 6, raw)


if __name__ == "__main__":
    icon("tex_dev_card.png", card)
    icon("tex_dev_usb.png", usb)
    icon("tex_dev_internal.png", internal)
    icon("tex_dev_system.png", system)
    icon("tex_dev_game.png", game)
    icon("tex_dev_network.png", network)
    icon("tex_checkbox_off.png", checkbox_off)
    icon("tex_checkbox_on.png", checkbox_on)
    neutral()
    print("ok")
