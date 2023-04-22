#!/bin/bash
export XDG_RUNTIME_DIR=/run/user/1000
weston --backend=drm-backend.so --tty="3" --config="/home/pi/build/2038-AugmentedNormalcy/run/weston.ini" &
sleep 5s # could be less
export WAYLAND_DISPLAY=wayland-0
export DISPLAY=:0
exec "/home/pi/build/2038-AugmentedNormalcy/bin/app_headset"
