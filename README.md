# AutoDeafenWayland

**The python helper, geode mod, and (obviously) ydotool are only expected to function in a Linux environment with the Wayland compositor, running Geometry Dash through wine. With that logic it's safe to assume this probably won't work on x11, either.**

If you're looking for a windows-native compatible auto-deafen, see: [Lynxdeer AutoDeafen](https://github.com/Lynxdeer/AutoDeafen)

it probably won't work on x11, either, but who knows........


<sub>It is also important to note that this has only been tested on Arch Linux in the KDE Plasma environment, but I suspect most environments on wayland should work ok. </sub>

_____

The Megahack & Geode auto-deafen do not work in Geometry Dash on wayland. This is due to an intentional security feature which blocks global synthetic inputs, which prevents your discord client on Linux from picking up any sort of input signal.

The purpose of the python helper and geode mod are to create a bridge between your geometry dash client and the wayland compositor itself, listening to a port that the mod will signal to when certain conditions are met, such as when you have reached your deafen %, or have paused the game (given you have this option enabled).

# Features
* Configurable deafen %
* Enable/Disable for Startpos/Practice
* Undeafen on pause

There is currently no baked in feature to change the keybind, you must change the keybind in the helper script yourself.

<img src="logo.png" width="150" alt="the mod's logo" />

*Update logo.png to change your mod's icon (please)*
