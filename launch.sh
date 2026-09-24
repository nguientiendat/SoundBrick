#!/bin/sh

# Dừng tiến trình giao diện chính của TrimUI (thường là MainUI) để tránh xung đột màn hình
killall -STOP MainUI 2>/dev/null
killall -STOP minigui 2>/dev/null

# Chuyển đến thư mục chứa app
APP_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$APP_DIR"

# Chạy ứng dụng SoundBrick
./soundbrick

# Khôi phục lại giao diện chính sau khi thoát app
killall -9 tplayerdemo 2>/dev/null
killall -9 tail 2>/dev/null
killall -CONT MainUI 2>/dev/null
killall -CONT minigui 2>/dev/null
