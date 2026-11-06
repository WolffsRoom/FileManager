#!/bin/bash
cd /mnt/c/Users/wolff/Documents/SDKVita/VitaShell-master
git ls-files --stage | grep "thehero" | head -3
echo "=== staged count ==="
git diff --cached --name-only | wc -l
echo "=== thehero staged ==="
git diff --cached --name-only | grep thehero | wc -l
