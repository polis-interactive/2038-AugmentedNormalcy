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


## Bringing up a pi

- install vim, git, cmake, build-essential
- Basic set up pi
- install boost (see guide)
- install libcamera-dev (camera only)
- install glfw (headset only, see guide), weston
- Set static ip (100's for headset, 200's for camera)

```
$ sudo vim /etc/dhcpcd.conf

interface wlan0
static ip_address=192.168.1.XXX/24
static routers=192.168.1.1

interface eth0
static ip_address=192.168.1.XXX/24
static routers=192.168.1.1
```

- Set WLANS for easy change over (headset only)

```
$ sudo vim /etc/wpa_supplicant/wpa_supplicant.conf
```

## SYSTEMD

```
$ sudo cp /home/pi/build/2038-AugmentedNormalcy/systemd/headset.service /etc/systemd/system/ [REPLACE headset WITH camera OR server]
$ sudo cp /home/pi/build/2038-AugmentedNormalcy/systemd/weston.service /etc/systemd/system/ [headset ONLY]
$ sudo cp /home/pi/build/2038-AugmentedNormalcy/systemd/restart-system.service /etc/systemd/system/
$ sudo systemctl daemon-reload
$ sudo systemctl enable weston.service [headset only]
$ sudo systemctl enable headset.service [REPLACE camera WITH headset OR server]
$ sudo systemctl enable restart-system.service
```

After, sudo reboot and make sure it works!