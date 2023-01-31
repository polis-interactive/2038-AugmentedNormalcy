#!/bin/bash
weston --backend=drm-backend.so &
sleep 5s # could be less
export WAYLAND_DISPLAY=wayland-0
export DISPLAY=:0
exec ./build/glfw_test
