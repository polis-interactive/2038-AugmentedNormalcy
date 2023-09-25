# Get it going

Follow instructions here: https://docs.nvidia.com/jetson/archives/r35.3.1/DeveloperGuide/text/IN/QuickStart.html

Getting the thing in forced recovery:

- have it on
- hold the reset (middle) button
- touch the restart (rightmost) button
- release the reset (middle) button

You'll need these downloaded:

BSP: https://developer.nvidia.com/downloads/embedded/l4t/r35_release_v3.1/sources/public_sources.tbz2/
Filesystem: https://developer.nvidia.com/downloads/embedded/l4t/r35_release_v3.1/sources/ubuntu_focal-l4t_aarch64_src.tbz2/

Get on wifi and set the static IP using the network manager

Change the power mode to max

Go to power settings, have it never turn off, never turn off wifi

# installing things

```
$ sudo apt-get update
$ sudo apt-get upgrade
$ sudo reboot
$ sudo apt install nvidia-jetpack
$ sudo apt-get install cmake nload
$ mkdir build
$ cd build
$ wget -O boost_1_81_0.tar.gz https://boostorg.jfrog.io/artifactory/main/release/1.81.0/source/boost_1_81_0.tar.gz
$ tar -xvzf boost_1_81_0.tar.gz
$ cd boost_1_81_0
$ ./bootstrap.sh gcc
$ ./b2 --with-system --with-thread --with-date_time --with-regex --with-serialization stage
$ cd ..
$ git clone https://github.com/polis-interactive/2038-AugmentedNormalcy
$ cd 2038-AugmentedNormalcy
$ mkdir build
$ cd build
$ cmake -DAN_PLATFORM:STRING=JETSON ..
$ make -j6
```