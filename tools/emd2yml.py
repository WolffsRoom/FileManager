#!/usr/bin/env python3
"""
emd2yml.py - converte os .emd do ScePaf-RE (formato do SDK oficial da Sony) para
o .yml de NIDs do vitasdk, permitindo gerar as bibliotecas de stub e linkar
codigo paf com a toolchain aberta.

Por que isso existe: o ScePaf-RE distribui os simbolos em .emd, que so o SDK
oficial consome. O vitasdk quer .yml com os NIDs explicitos. O .emd traz os
nomes mas nao os NIDs -- eles precisam ser calculados.

ALGORITMO DO NID (confirmado experimentalmente):

    NID = primeiros 4 bytes de SHA1(simbolo + nidsuffix), lidos little-endian

Verificacao feita contra o proprio vitasdk, que ja publica NIDs de ScePafMisc e
ScePafResource. Cinco de cinco bateram, e os nomes amigaveis do vitasdk casam
semanticamente com os simbolos C++ mangled do .emd:

    scePafResourceGetAttribute       0xAD8678FB
      = paf::cxml::Element::GetAttribute(char const*, paf::cxml::Attribute*)
    scePafResourceGetAttributeFloat  0xBE772869
      = paf::cxml::util::GetFloat(paf::cxml::Element const&, char const*, float*)

Os NIDs de nome de biblioteca (`libnamenid` no .emd) tambem batem: 3/3, incluindo
ScePafWidget = 0x073F8C68.

Uso:
    python tools/emd2yml.py ScePaf-RE-main/lib -o app/nids
"""

import argparse
import hashlib
import os
import re
import struct
import sys

# NID do modulo. Todas as bibliotecas ScePaf* sao exportadas por ScePaf.suprx;
# o valor vem do proprio vitasdk (share/vita-headers/db/360/ScePaf.yml).
MODULOS = {
    "ScePaf": (0xCD679177, [
        "ScePafCommon", "ScePafGraphics", "ScePafLowlayer", "ScePafMisc",
        "ScePafResource", "ScePafStdc", "ScePafThread", "ScePafToplevel",
        "ScePafWidget",
    ]),
}

# Bibliotecas que moram em outros modulos, cujo NID de modulo nao conhecemos.
# Ficam de fora por padrao para nao gravar um valor inventado.
FORA = {"SceAppSettings", "SceCommonGuiDialog", "SceWebUIPlugin", "ScePafWebMapView"}

FIRMWARE = "3.60"


def nid(simbolo, sufixo):
    """SHA1(simbolo + sufixo)[0:4] lido como u32 little-endian."""
    digest = hashlib.sha1((simbolo + sufixo).encode("ascii")).digest()
    return struct.unpack("<I", digest[:4])[0]


def ler_emd(caminho):
    """Devolve {'nome', 'nidsuffix', 'libnamenid', 'functions', 'variables'}."""
    texto = open(caminho, encoding="utf-8", errors="replace").read()

    nome = None
    m = re.search(r"Library:\s*(\S+)", texto)
    if m:
        nome = m.group(1)

    sufixo = ""
    m = re.search(r'nidsuffix:\s*"([^"]*)"', texto)
    if m:
        sufixo = m.group(1)

    libnid = None
    m = re.search(r"libnamenid:\s*(0x[0-9A-Fa-f]+)", texto)
    if m:
        libnid = int(m.group(1), 16)

    funcs, vars_ = [], []
    for linha in texto.splitlines():
        mf = re.search(r"function:\s*(\S+)", linha)
        if mf:
            explicito = re.search(r"nidvalue:\s*(0x[0-9A-Fa-f]+)", linha)
            funcs.append((mf.group(1), int(explicito.group(1), 16) if explicito else None))
            continue
        mv = re.search(r"variable:\s*(\S+)", linha)
        if mv:
            explicito = re.search(r"nidvalue:\s*(0x[0-9A-Fa-f]+)", linha)
            vars_.append((mv.group(1), int(explicito.group(1), 16) if explicito else None))

    return {
        "nome": nome, "sufixo": sufixo, "libnamenid": libnid,
        "functions": funcs, "variables": vars_,
    }


def conferir(lib):
    """O libnamenid do .emd tem que bater com o algoritmo. Se nao bater, avisa."""
    if lib["libnamenid"] is None:
        return True
    calculado = nid(lib["nome"], lib["sufixo"])
    if calculado != lib["libnamenid"]:
        print(f"  AVISO {lib['nome']}: libnamenid 0x{lib['libnamenid']:08X} "
              f"!= calculado 0x{calculado:08X}", file=sys.stderr)
        return False
    return True


def emitir(modulo, modnid, libs):
    linhas = ["version: 2", f"firmware: {FIRMWARE}", "modules:",
              f"  {modulo}:", f"    nid: 0x{modnid:08X}", "    libraries:"]
    for lib in libs:
        linhas.append(f"      {lib['nome']}:")
        linhas.append("        kernel: false")
        linhas.append(f"        nid: 0x{nid(lib['nome'], lib['sufixo']):08X}")
        if lib["functions"]:
            linhas.append("        functions:")
            for simbolo, explicito in sorted(set(lib["functions"])):
                valor = explicito if explicito is not None else nid(simbolo, lib["sufixo"])
                linhas.append(f"          {simbolo}: 0x{valor:08X}")
        if lib["variables"]:
            linhas.append("        variables:")
            for simbolo, explicito in sorted(set(lib["variables"])):
                valor = explicito if explicito is not None else nid(simbolo, lib["sufixo"])
                linhas.append(f"          {simbolo}: 0x{valor:08X}")
    return "\n".join(linhas) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("entrada", help="pasta com os .emd")
    ap.add_argument("-o", "--saida", default="app/nids")
    ap.add_argument("--todos", action="store_true",
                    help="inclui bibliotecas de modulos cujo NID nao conhecemos")
    args = ap.parse_args()

    os.makedirs(args.saida, exist_ok=True)

    libs = {}
    for nome_arq in sorted(os.listdir(args.entrada)):
        if not nome_arq.endswith(".emd"):
            continue
        lib = ler_emd(os.path.join(args.entrada, nome_arq))
        if not lib["nome"]:
            continue
        conferir(lib)
        libs[lib["nome"]] = lib

    total_f = total_v = 0
    for modulo, (modnid, membros) in MODULOS.items():
        presentes = [libs[n] for n in membros if n in libs]
        if not presentes:
            continue
        destino = os.path.join(args.saida, f"{modulo}.yml")
        with open(destino, "w", encoding="utf-8") as f:
            f.write(emitir(modulo, modnid, presentes))
        for lib in presentes:
            total_f += len(set(lib["functions"]))
            total_v += len(set(lib["variables"]))
        print(f"  {destino}")
        for lib in presentes:
            print(f"      {lib['nome']:<20} {len(set(lib['functions'])):>5} func  "
                  f"{len(set(lib['variables'])):>4} var")

    ignoradas = [n for n in libs if n in FORA]
    if ignoradas and not args.todos:
        print(f"\n  fora (NID de modulo desconhecido): {', '.join(sorted(ignoradas))}")

    print(f"\n{total_f} funcoes e {total_v} variaveis convertidas")


if __name__ == "__main__":
    main()
