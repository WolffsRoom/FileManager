#!/usr/bin/env python3
"""
make_livearea.py - gera os assets de sce_sys do File Manager (herdado do projeto Files) na mesma linguagem visual
do app: bolha circular com verniz, sobre o azul do sistema.

    python tools/make_livearea.py app/CONTENTS/sce_sys

Tamanhos exigidos pelo Vita:
    icon0.png                        128 x 128
    livearea/contents/gate.png       280 x 158
    livearea/contents/labg.png       840 x 500
    livearea/contents/title.png      377 x  70
    pic0.png                         960 x 544
"""

import os
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

K = 4  # supersampling

AZUL_ESCURO = (7, 30, 66)
AZUL_MEDIO = (18, 68, 132)
AZUL_CLARO = (58, 140, 210)
BOLHA = (47, 110, 168)


def gradiente(size, topo, base):
    w, h = size
    img = Image.new("RGB", (1, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        d.point((0, y), tuple(round(topo[i] + (base[i] - topo[i]) * t) for i in range(3)))
    return img.resize((w, h), Image.BILINEAR).convert("RGBA")


def bolha(diam, cor=BOLHA, com_pasta=True):
    """Uma bolha circular com verniz, do mesmo desenho que paf-theme.js faz."""
    s = diam * K
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # corpo com gradiente vertical, recortado no circulo
    corpo = gradiente((s, s),
                      tuple(min(255, round(c + (255 - c) * 0.26)) for c in cor),
                      tuple(round(c * 0.70) for c in cor))
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).ellipse([0, 0, s - 1, s - 1], fill=255)
    img.paste(corpo, (0, 0), mask)

    # calota de verniz no alto
    verniz = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    vd = ImageDraw.Draw(verniz)
    vd.ellipse([s * 0.06, -s * 0.12, s * 0.94, s * 0.42], fill=(255, 255, 255, 96))
    verniz = verniz.filter(ImageFilter.GaussianBlur(s * 0.05))
    img.alpha_composite(Image.composite(verniz, Image.new("RGBA", (s, s), (0, 0, 0, 0)), mask))

    # borda clara
    d.ellipse([1, 1, s - 2, s - 2], outline=(255, 255, 255, 120), width=max(1, s // 90))

    if com_pasta:
        # glifo de pasta, centrado
        gw, gh = s * 0.46, s * 0.36
        gx, gy = (s - gw) / 2, (s - gh) / 2 + s * 0.02
        aba = gh * 0.22
        d.polygon([(gx, gy + aba), (gx + gw * 0.40, gy + aba),
                   (gx + gw * 0.48, gy), (gx + gw * 0.92, gy),
                   (gx + gw * 0.92, gy + aba)], fill=(255, 255, 255, 240))
        d.rounded_rectangle([gx, gy + aba, gx + gw, gy + gh],
                            radius=s * 0.02, fill=(255, 255, 255, 240))

    return img.resize((diam, diam), Image.LANCZOS)


def fonte(px, bold=True):
    for nome in (("segoeuib.ttf", "segoeui.ttf") if bold else ("segoeui.ttf",)):
        try:
            return ImageFont.truetype(nome, px)
        except OSError:
            pass
    return ImageFont.load_default()


def centralizar(base, sobre, dy=0):
    x = (base.width - sobre.width) // 2
    y = (base.height - sobre.height) // 2 + dy
    base.alpha_composite(sobre, (x, y))


def icon0(path):
    img = gradiente((128, 128), AZUL_MEDIO, AZUL_ESCURO)
    centralizar(img, bolha(104))
    img.save(path)
    print("  icon0.png            128x128")


def gate(path):
    img = gradiente((280, 158), AZUL_MEDIO, AZUL_ESCURO)
    centralizar(img, bolha(96), dy=-14)
    d = ImageDraw.Draw(img)
    f = fonte(26)
    texto = "File Manager"
    w = d.textlength(texto, font=f)
    d.text(((280 - w) / 2, 116), texto, font=f, fill=(255, 255, 255, 235))
    img.save(path)
    print("  gate.png             280x158")


def labg(path):
    img = gradiente((840, 500), AZUL_CLARO, AZUL_ESCURO)
    d = ImageDraw.Draw(img)
    # bolhas de fundo, bem apagadas, como enfeite
    for cx, cy, r, a in ((120, 400, 150, 26), (700, 90, 200, 20), (430, 470, 110, 16)):
        camada = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ImageDraw.Draw(camada).ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 255, 255, a))
        img.alpha_composite(camada)
    img.save(path)
    print("  labg.png             840x500")


def title(path):
    img = Image.new("RGBA", (377, 70), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    f = fonte(48)
    d.text((0, 6), "File Manager", font=f, fill=(255, 255, 255, 245))
    f2 = fonte(18, bold=False)
    d.text((4, 56), "Gerenciador de Arquivos", font=f2, fill=(210, 232, 255, 200))
    img.save(path)
    print("  title.png            377x70")


def pic0(path):
    img = gradiente((960, 544), AZUL_MEDIO, AZUL_ESCURO)
    centralizar(img, bolha(200), dy=-30)
    img.save(path)
    print("  pic0.png             960x544")


TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<livearea style="a1" format-ver="01.03" content-rev="1">

\t<livearea-background>
\t\t<image>labg.png</image>
\t</livearea-background>

\t<gate>
\t\t<startup-image>gate.png</startup-image>
\t</gate>

\t<frame id="frame3">
\t\t<liveitem default="on">
\t\t\t<image valign="top">title.png</image>
\t\t</liveitem>
\t</frame>
</livearea>
"""


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "app/CONTENTS/sce_sys"
    la = os.path.join(out, "livearea", "contents")
    os.makedirs(la, exist_ok=True)
    print("gerando em", out)
    icon0(os.path.join(out, "icon0.png"))
    pic0(os.path.join(out, "pic0.png"))
    gate(os.path.join(la, "gate.png"))
    labg(os.path.join(la, "labg.png"))
    title(os.path.join(la, "title.png"))
    with open(os.path.join(la, "template.xml"), "w", encoding="utf-8") as f:
        f.write(TEMPLATE)
    print("  template.xml")


if __name__ == "__main__":
    main()
