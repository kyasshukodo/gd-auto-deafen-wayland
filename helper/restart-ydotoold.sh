#!/usr/bin/env bash

set -e

echo "[*] Stopping any existing ydotoold..."
sudo pkill ydotoold 2>/dev/null || true

echo "[*] Removing stale socket (if any)..."
sudo rm -f /tmp/.ydotool_socket

echo "[*] Starting ydotoold..."
sudo ydotoold &

sleep 0.5

echo "[*] Current ydotoold processes:"
ps aux | grep ydotoold | grep -v grep

echo "[*] Done."
