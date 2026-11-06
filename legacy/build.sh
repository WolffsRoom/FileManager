#!/bin/bash
# Script de build para VitaShell (FileManager)

clear

echo "============================================="
echo "   Compilando VitaShell (Modificado)         "
echo "============================================="

# Verifica se a variável de ambiente VITASDK está definida
if [ -z "$VITASDK" ]; then
    echo "Erro: A variavel de ambiente \$VITASDK nao esta configurada."
    echo "Certifique-se de que exportou: export VITASDK=/usr/local/vitasdk"
    exit 1
fi

# Cria diretório de build se não existir e entra nele
mkdir -p build
cd build

# Executa o CMake apontando para a toolchain específica do Vita
echo "[1/3] Configurando o CMake com a toolchain do Vita..."
cmake -DCMAKE_TOOLCHAIN_FILE=${VITASDK}/share/vita.toolchain.cmake ..

# Compila e empacota o VPK
echo "[2/3] Compilando e empacotando o VPK..."
make

# Verifica o resultado
if [ -f "FileManager.vpk" ]; then
    echo "============================================="
    echo "Sucesso! O arquivo VPK foi gerado com exito!"
    echo "Localizacao do arquivo: $(pwd)/FileManager.vpk"
    echo "============================================="
else
    echo "Erro: A compilacao falhou ou o arquivo .vpk nao foi gerado."
fi
