<div align="center">

# 🧱 SoundBrick 📻

**A Retro, Ultra-Lightweight SoundCloud Player for TrimUI Brick Pro**

![Platform](https://img.shields.io/badge/Platform-TrimUI%20Brick%20Pro-orange?style=for-the-badge&logo=linux)
![Language](https://img.shields.io/badge/Language-C++-blue?style=for-the-badge&logo=c%2B%2B)
![Status](https://img.shields.io/badge/Status-Beta-green?style=for-the-badge)

</div>

## 🌟 What is SoundBrick?

**SoundBrick** is a bare-metal, hardware-accelerated music player built from scratch for the **TrimUI Smart Pro (Allwinner A133P)**.
Tired of heavy Electron apps, ads, and login screens? SoundBrick gives you pure Lofi vibes right on your retro gaming handheld with zero friction.

### ✨ Features

- 🕵️ **Anonymous Streaming:** No SoundCloud account required! Just boot up and search.
- 🎨 **Dynamic Album UI:** Extracts the dominant color from the track's album art and generates a smooth Spotify-like background gradient.
- 🔋 **Extreme Battery Saver Mode:** Press `SELECT` to instantly freeze the GPU and CPU rendering loop, putting the screen to sleep while the music keeps playing. Saves up to 50% battery!
- 🎮 **Native D-Pad Controls:** Fully mapped hardware buttons for skipping tracks, pausing, and searching with an integrated On-Screen Keyboard.
- 🚀 **Zero Dependencies:** Written in raw C++ & SDL2, compiled natively for the device.

---

## 📖 User Guide

### 1. First Boot & Setup

When you launch SoundBrick for the very first time, you will be greeted by the **SOUNDCLOUD LOGIN** screen. Don't panic! You don't need a real account:

- Simply press the **Y button** on your TrimUI.
- You will be immediately redirected to the Search screen.

_(If you ever need to manually update or re-fetch the ID later, press **Y** from the Search screen)._

### 2. Controls & Navigation

SoundBrick is designed entirely around the TrimUI physical buttons. No touch screen required!

| Button                 | Search Screen (OSK)             | Now Playing Screen            |
| :--------------------- | :------------------------------ | :---------------------------- |
| **D-PAD**              | Move keyboard cursor            | Skip to Next / Previous Track |
| **A Button** _(East)_  | Type selected letter / Press GO | Play / Pause Music            |
| **B Button** _(South)_ | Delete letter (Backspace)       | _N/A_                         |
| **Y Button** _(West)_  | Open ID Config / Auto Fetch     | _N/A_                         |
| **SELECT**             | Screen Off (Battery Saver)      | Screen Off (Battery Saver)    |
| **START**              | Exit App                        | Exit App                      |

### 3. Battery Saver Mode

Pressing **SELECT** will immediately freeze all graphical rendering and turn the display pitch black while the music continues playing smoothly in the background. This drastically reduces CPU and GPU usage, saving up to 50% battery for long Lo-fi listening sessions! Press any button to wake the screen up.

---

## 🛠️ How to Build

SoundBrick requires a cross-compilation toolchain for `aarch64-linux-gnu` (TrimUI Smart Pro architecture).

```bash
# Clone the repository
git clone https://github.com/YOUR_NAME/SoundBrick.git
cd SoundBrick

# Create build directory
mkdir build && cd build

# Cross-compile using the provided toolchain
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain/aarch64-linux.cmake ..
make -j$(nproc)
```

## 🚀 Installation

1. Copy the compiled `soundbrick` binary to your SD Card (e.g., `/mnt/SDCARD/Apps/SoundBrick/`).
2. Ensure the execution permission is granted: `chmod +x soundbrick`.
3. Launch `launch.sh` from CrossMix OS or StockOS!

---

<div align="center">
<i>Built with ❤️ for the Retro Gaming Community</i>
</div>
