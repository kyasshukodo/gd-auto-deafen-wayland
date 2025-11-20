# Auto Deafen Wayland

The Megahack & Geode auto-deafen do not work in Geometry Dash on wayland. This is due to an intentional security feature which blocks global synthetic inputs, which prevents your discord client on Linux from picking up any sort of input signal.

The purpose of the python helper and geode mod are to create a bridge between your geometry dash client and the wayland compositor.