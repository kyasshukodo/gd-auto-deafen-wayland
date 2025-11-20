# AutoDeafenWayland

**The python helper, geode mod, and (obviously) ydotool are only expected to function in a Linux environment with the Wayland compositor, running Geometry Dash through wine. This mod will _NOT_ work on x11!**

If you're running windows and are looking for a windows-native compatible auto-deafen, see: [Lynxdeer AutoDeafen](https://github.com/Lynxdeer/AutoDeafen)

<sub>It is also important to note that this has only been tested on Arch Linux in the KDE Plasma environment, but I suspect most environments on wayland should work ok. </sub>

_____

The Megahack & Geode auto-deafen do not work in Geometry Dash on wayland. This is due to an intentional security feature which blocks global synthetic inputs, which prevents your discord client on Linux from picking up any sort of input signal.

The purpose of the python helper and geode mod are to create a bridge between your geometry dash client and the wayland compositor.
## Dependencies

- **Python 3** is needed for the helper script.
- **ydotool** and **ydotoold** are needed to process the keybinds for discord.

### Installing ydotool

**Arch Linux:**
```bash
sudo pacman -S ydotool
```

**Ubuntu/Debian:**
```bash
sudo apt install ydotool
```

**Fedora:**
```bash
sudo dnf install ydotool
```

**From source:**
```bash
git clone https://github.com/ReimuNotMoe/ydotool
cd ydotool
mkdir build && cd build
cmake ..
make
sudo make install
```
## Setup
- Drag the .geode file into your geode mods folder.
- Run the provided python script in sudo:
```bash
# ./gd_deafen_helper.py
```
If the geode mod is properly installed in your game, it will check for a port connection anytime you enter the main menu. If it cannot ping the listener through the port, a pop-up will appear in your main menu alerting you so.

## Change Keybind

- Open `helper/gd_deafen_helper.py`.
- Find the `COMMANDS["DEAFEN"]` entry and replace the numeric key sequence with your desired combo.
- Use `1` for key down and `0` for key up. Common codes would be: `Ctrl=29`, `Shift=42`, `D=32`, `M=50`.
- Example (Ctrl+Shift+M):
	```python
	COMMANDS = {
			"DEAFEN": [
					"ydotool", "key",
					"29:1", "42:1", "50:1", "50:0", "42:0", "29:0"
			],
	}
	```
- Restart the helper after saving changes.
> [!NOTE]
> ydotool uses linux evdev key codes to specify what keys are to be pressed.


## Features
* Configurable deafen %
* Enable/Disable for Startpos/Practice
* Undeafen on pause
* Auto-managing ydotoold daemon within helper

I hope to implement in-game keybind mapping in the future.

<img src="logo.png" width="150" alt="the mod's logo" />
