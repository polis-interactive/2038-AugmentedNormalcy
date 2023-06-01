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
    (or cmake -DAN_PLATFORM:STRING=RPI_DISPLAY ..)
$ make -j4
```

## RPI debug

```
$ sudo modprobe -r bcm2835-codec
$ sudo modprobe bcm2835-codec debug=5
$ dmesg
```

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

## SYSTEM

```
$ sudo cp /home/pi/build/2038-AugmentedNormalcy/system/headset.service /etc/systemd/system/ [REPLACE headset WITH camera OR server]
$ sudo cp /home/pi/build/2038-AugmentedNormalcy/system/weston.service /etc/systemd/system/ [headset ONLY]
$ sudo cp /home/pi/build/2038-AugmentedNormalcy/system/restart-system.service /etc/systemd/system/
$ sudo systemctl daemon-reload
$ sudo systemctl enable weston.service [headset only]
$ sudo systemctl enable headset.service [REPLACE camera WITH headset OR server]
$ sudo cp /home/pi/build/2038-AugmentedNormalcy/system/cron-reboot /etc/cron.d/reboot
```

After, sudo reboot and make sure it works!

## Packer

```
$ sudo -E $(which packer) build camera.pkr.hcl
$ sudo -E $(which packer) build headset.pkr.hcl
$ sudo -E $(which packer) build display.pkr.hcl
```

## Edit the pi
```
$ sudo vim /media/brucegoose/rootfs/etc/hostname 
$ sudo vim /media/brucegoose/rootfs/etc/hosts
$ sudo vim /media/brucegoose/rootfs/etc/dhcpcd.conf 
# camera only
$ sudo vim /media/brucegoose/rootfs/etc/systemd/system/camera.service
```