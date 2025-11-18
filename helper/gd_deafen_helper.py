#!/usr/bin/env python3
import socket
import subprocess

HOST = "127.0.0.1"
PORT = 44555

# Correct numeric keycodes for Ctrl+Shift+D:
#   29:1 42:1 32:1 32:0 42:0 29:0
COMMANDS = {
    "DEAFEN": [
        "ydotool", "key",
        "29:1", "42:1", "32:1", "32:0", "42:0", "29:0"
    ],
}

def handle_command(cmd: str):
    cmd = cmd.strip().upper()
    if cmd in COMMANDS:
        try:
            subprocess.run(COMMANDS[cmd], check=True)
            print(f"Executed command: {cmd}", flush=True)
        except subprocess.CalledProcessError as e:
            print(f"Failed to execute {cmd}: {e}", flush=True)
    else:
        print(f"Unknown command: {cmd!r}", flush=True)

def main():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen(5)
        print(f"Helper listening on {HOST}:{PORT}...", flush=True)
        while True:
            conn, addr = s.accept()
            with conn:
                data = b""
                while True:
                    chunk = conn.recv(1024)
                    if not chunk:
                        break
                    data += chunk
                    while b"\n" in data:
                        line, data = data.split(b"\n", 1)
                        if line:
                            handle_command(line.decode("utf-8", errors="ignore"))

if __name__ == "__main__":
    main()
