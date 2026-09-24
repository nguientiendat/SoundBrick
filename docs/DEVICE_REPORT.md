# TrimUI Brick Pro - Device Report

## System Overview
- **OS**: Linux TinaLinux 4.9.191 (Allwinner SDK)
- **Architecture**: `aarch64` (ARM64)
- **CPU**: ARM Cortex-A53 (4 cores)
- **RAM**: ~1GB (975 MB total, plenty of available RAM for our needs)
- **Storage**: SD Card is mounted at `/mnt/SDCARD` (exFAT)
- **C Library**: `glibc 2.33` (located at `/lib/libc-2.33.so`)

## Hardware & Libraries
- **Audio**: Standard Allwinner ALSA device (`audiocodec`).
- **Input**: `/dev/input/js0` and `event0`-`event3` are available. SDL2 should pick these up automatically via the evdev/joystick subsystem.
- **Graphics & SDL2**: SDL2, SDL2_image, SDL2_ttf, and SDL2_mixer are installed on the system (via opkg). This means we can dynamically link `libSDL2` without bundling it.

## Media & Networking Available
- **Networking**: `curl` and `wget` are available natively.
- **FFmpeg**: `/usr/bin/ffmpeg`, `libavcodec.so.60`, `libavformat.so.60` (FFmpeg 6.0) are present. `ffplay` is missing.
- **Hardware Player**: `tplayerdemo` and `libcedarx` are present.
- **mpv**: Not found.

## Actionable Conclusions
1. **Toolchain**: We must use an `aarch64-linux-gnu` toolchain based on glibc <= 2.33.
2. **Audio Player Backend**: 
   - Option A: Use `tplayerdemo` via `fork()/exec()`. We need to verify if it supports HTTPS streams.
   - Option B: Write a custom player using the available `libavformat`/`libavcodec` + SDL2 Audio.
   - Option C: `ffmpeg` CLI pipe to ALSA (fallback).
3. **HTTP Client**: We can either static link `libcurl` or use the system's `libcurl.so` if available (needs verification, but `/usr/bin/curl` exists). To be safe and modular, we will build a C++ HTTP client wrapper around `libcurl`.
4. **App Path**: The SD Card is at `/mnt/SDCARD/`, so our `launch.sh` will reside there.
