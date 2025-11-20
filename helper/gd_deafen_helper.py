#!/usr/bin/env python3
import socket
import subprocess
import os
import signal
import time

HOST = "127.0.0.1"
PORT = 44555

# set the socket path for ydotool to use
YDOTOOL_SOCKET = "/tmp/.ydotool_socket"
os.environ["YDOTOOL_SOCKET"] = YDOTOOL_SOCKET


# this is where you will change your keybind.
# refer to the linux evdev key codes to figure out what to put here.
# make sure for every keyon signal there is a keyoff signal, or else this will not work.
# I will try to make a better solution for this!! Thank you for bearing with me......
COMMANDS = {
    "DEAFEN": [
        "ydotool", "key",
        "29:1", "42:1", "32:1", "32:0", "42:0", "29:0"
    ],
}

def manage_ydotoold_daemon(action: str):
    """
    manage the ydotoold daemon.
    actions: START, STOP, RESTART, STATUS
    """
    try:
        if action == "STOP":
            print("[*] stopping ydotoold daemon...")
            subprocess.run(["sudo", "pkill", "ydotoold"], 
                         stderr=subprocess.DEVNULL, check=False)
            time.sleep(0.3)
            print("[+] ydotoold stopped")
            
        elif action == "START":
            print("[*] starting ydotoold daemon...")
            # remove stale socket
            subprocess.run(["sudo", "rm", "-f", YDOTOOL_SOCKET],
                         stderr=subprocess.DEVNULL, check=False)
            # start daemon with explicit socket path
            subprocess.Popen(["sudo", "ydotoold", "--socket-path", YDOTOOL_SOCKET],
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
            time.sleep(0.5)
            print("[+] ydotoold started")
            
        elif action == "RESTART":
            print("[*] restarting ydotoold daemon...")
            manage_ydotoold_daemon("STOP")
            manage_ydotoold_daemon("START")
            print("[+] ydotoold restarted")
            
        elif action == "STATUS":
            print("[*] checking ydotoold status...")
            result = subprocess.run(["pgrep", "-a", "ydotoold"],
                                  capture_output=True, text=True)
            if result.returncode == 0:
                print(f"[+] ydotoold is running:\n{result.stdout.strip()}")
            else:
                print("[!] ydotoold is not running")
                
            # check socket
            socket_exists = os.path.exists(YDOTOOL_SOCKET)
            print(f"[*] socket status: {'exists' if socket_exists else 'missing'}")
            
        else:
            print(f"[!] unknown daemon action: {action}")
            
    except Exception as e:
        print(f"[!] error managing daemon: {e}")

def handle_command(cmd: str):
    cmd = cmd.strip().upper()
    if cmd == "PING":
        print("[+] connection established with autodeafen")
    elif cmd in COMMANDS:
        try:
            subprocess.run(COMMANDS[cmd], check=True, env=os.environ,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print(f"[+] executed command: {cmd}")
        except subprocess.CalledProcessError as e:
            print(f"[-] failed to execute {cmd}: {e}")
            # auto-restart daemon if key command fails
            print("[*] attempting to restart ydotoold...")
            manage_ydotoold_daemon("RESTART")
    elif cmd.startswith("DAEMON_"):
        # handle daemon commands: DAEMON_START, DAEMON_STOP, DAEMON_RESTART, DAEMON_STATUS
        action = cmd.replace("DAEMON_", "")
        manage_ydotoold_daemon(action)
    else:
        print(f"[?] unknown command: {cmd!r}")

def main():
    # auto-start daemon on helper startup
    print("[*] initializing gd_deafen_helper...")
    print(f"[*] using socket path: {YDOTOOL_SOCKET}")
    manage_ydotoold_daemon("STATUS")
    
    # start if not running
    result = subprocess.run(["pgrep", "ydotoold"], 
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        print("[*] ydotoold not running, starting...")
        manage_ydotoold_daemon("START")
    else:
        print("[+] ydotoold already running")
    
    print(f"\n[+] helper listening on {HOST}:{PORT}...\n")
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen(5)
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
