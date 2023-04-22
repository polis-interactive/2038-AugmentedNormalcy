# 2038-AugmentedNormalcy

## Mounting jetson image

Getting cross compiling going was a nightmare; had to follow this mainly, but stumbled alot along the way:
https://forums.developer.nvidia.com/t/question-about-cross-compilation-link-errors/238711/7

```
$ sudo mount ~/build/Linux_for_Tegra/jetson-cc.img.raw ~/cross-compile/jetson-orin/rootfs
```

## Compiling on jetson

```
$ mkdir build
$ cd build
$ cmake -DAN_PLATFORM:STRING=JETSON ..
$ make -j4
```

## Compiling on RPI

```
$ mkdir build
$ cd build
$ cmake -DAN_PLATFORM:STRING=RPI_CAMERA ..
    (or cmake -DAN_PLATFORM:STRING=RPI_HEADSET ..)
$ make -j4
```

## RPI debug

```
$ sudo modprobe -r bcm2835-codec
$ sudo modprobe bcm2835-codec debug=5
$ dmesg
```

## Bringing up a pi

- Basic set up pi
- install vim, git, cmake, build-essential, libcamera-dev
- install boost (see guide)
- install glfw (headset only, see guide), weston
- 

## Static IP on the pi

Add these lines to /etc/dhcpcd.conf

above 200 for camera, 100-200 for headset

```
interface wlan0
static ip_address=192.168.1.200/24
static routers=192.168.1.1

interface eth0
static ip_address=192.168.1.200/24
static routers=192.168.1.1
```