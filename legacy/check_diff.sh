#!/bin/bash
cd /mnt/c/Users/wolff/Documents/SDKVita/VitaShell-master
echo "=== thehero/main.c diff lines ==="
git diff "VitaShell-master(thehero)/main.c" | wc -l
echo "=== root main.c diff lines ==="
git diff main.c | wc -l
echo "=== root main.c diff ==="
git diff main.c
