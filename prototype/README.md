# Notes

## Installing cuda

DONT TRUST APT REPOSITORY PACKAGES. Install apt-purge all existing nvidia drivers and follow these instructions

https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html

## Compiling cuda

To compile for broose's ghetto laptop, the barely supported maxwell architecture, needed the following gencode added to the nvidia compiler:

```
$ nvcc -gencode=arch=compute_50,code=sm_50 hello.cu -o hello
```

## Video_Sdk Examples

The following gave p fkn good results for big bunny rendering at 60fps;

download big bunny from here: http://distribution.bbb3d.renderfarming.net/video/mp4/bbb_sunflower_1080p_60fps_normal.mp4

Get the video sdk from here: https://developer.nvidia.com/nvidia-video-codec-sdk/download

Choose the left option, complete video codec. You'll need to make an account

After that, there is a readme pdf that tells you all the deps you need. Getting cuda up and running may be painful
given how old the arch you are using. TBH I don't even remember what I did, I deleted and reinstalled nvidia drivers
multiple times to get it working. What I remember best is choosing xOrg driver from apt library, apt purging all all
nvidia driver, and installing the cuda toolkit from src.

```
$ ./AppTrans -i ~/Downloads/bbb_sunflower_1080p_60fps_normal.mp4 -tuninginfo ultralowlatency -fps 60 \
    -codec h264 -maxbitrate 2M -vbvbufsize 4M -rc vbr -profile baseline
```

```
# time: 2m2.778s, file size: 43.6MB, quality: dog shit
$ ./AppTrans -i ~/Downloads/bbb_sunflower_1080p_60fps_normal.mp4 -tuninginfo ultralowlatency -fps 60 \
    -codec h264 -maxbitrate 1M -vbvbufsize 16K -rc cbr -profile baseline -multipass qres

```

```
# time: 1m49.332s, file size: 152MB, quality: barely passable
$ time ./AppTrans -i ~/Downloads/bbb_sunflower_1080p_60fps_normal.mp4 -tuninginfo ultralowlatency -fps 60 \
    -codec h264 -maxbitrate 2M -vbvbufsize 4M -rc vbr -profile baseline -multipass qres -preset p1

```

```
# time: 1m56.398s, file size: 151.8MB, quality: passable
# Tried with vbv buff size 1M and didn't notice any difference at all; I think maxbitrate is the controller of
#   quality here
$ time ./AppTrans -i ~/Downloads/bbb_sunflower_1080p_60fps_normal.mp4 -tuninginfo ultralowlatency -fps 60 \
    -codec h264 -maxbitrate 2M -vbvbufsize 4M -rc vbr -profile baseline -multipass qres -preset p3

```

```
# time: 1m56.398s, file size: 227.5MB, quality: great
# Can't tell if the quality comes from cbr or maxbitrate 3M; nah, just tried with 2M maxbitrate and we are
#   chilling; has that 151.8MB size, pretty good quality. Maybe cbr is the way to go
$ time ./AppTrans -i ~/Downloads/bbb_sunflower_1080p_60fps_normal.mp4 -tuninginfo ultralowlatency -fps 60 \
-codec h264 -maxbitrate 3M -vbvbufsize 6M -rc cbr -profile baseline -multipass qres -preset p3
```
 
-fps 60 \
-codec h264 -maxbitrate 3M -vbvbufsize 6M

I think I need to turn vbv / bitrate down? nvidia is saying bitrate / framerate; at 3M that's 50K; I don't see
any real speed up though; file size is some bit smaller, but a couple times the quality is pretty bad; I don't
think that will matter as much though since so much of the image is static; we can probably tune on a channel
by channel basis for vbv buff

```
# time: 1m55.648s, file size: 138.2MB, quality: great
$ time ./AppTrans -i ~/Downloads/bbb_sunflower_1080p_60fps_normal.mp4 -tuninginfo ultralowlatency -fps 60 \
    -codec h264 -maxbitrate 2M -vbvbufsize 50K -rc cbr -profile baseline -multipass qres -preset p3

```

Wow, we probably don't need it, but maxbitrate at 8m gave me amazing output, and took the same time. file size is
a little bigger though

```
# time: 1m56.899s, file size: 526.6MB, quality: AMAZING
$ time ./AppTrans -i ~/Downloads/bbb_sunflower_1080p_60fps_normal.mp4 -tuninginfo ultralowlatency -fps 60 \
    -codec h264 -maxbitrate 2M -vbvbufsize 50K -rc cbr -profile baseline -multipass qres -preset p3

```

## Video Sdk local

To make sure I knew how to compile the video sdk from src, I took my working transcode decode example I created
and ported  it to scratch/video_sdk/local_app_transdec. Also, I ripped out everything that wasn't linux. 
Need ffmpeg to demux existing video files. The example was created by combining samples/appTranscode/AppTrans and 
AppDecode/AppDecGl. Mainly as a test to see if I actually needed the demuxer is I'm just passing around raw packets. You
don't :D

You'll need nvidia rendering to the screen for this to work as we use GLX to create a window and write directly to
it from the decoder. I accomplish this by using Nvidia PrimeSelect and choosing Nvidia (i.e., gpu is rendering everything)
as opposed to Nvidia OnDemand (nvidia drivers are running, can request run from them but generally graphics are handled
by builtin [intel / amd] drivers)

Its a cmake project so do the following to get it running:

```
$ cd ./scratch/video_sdk
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make
$ make install
# note, we have the same params as above; think they are winners, might need to change preset tho
$ ./local_app_trans_dec -i ~/Downloads/bbb_sunflower_1080p_60fps_normal.mp4 -tuninginfo ultralowlatency -fps 60  \
  -codec h264 -maxbitrate 2M -vbvbufsize 50K -rc cbr -profile baseline -multipass qres -preset p3
```

## Dependencies

Follow ./scripts/setup/install_boost.{sh,bat} for how to install boost

for sqllitecpp:
git submodule init
git submodule update

install cuda toolkit if that's your thing from here

https://developer.nvidia.com/cuda-downloads?target_os=Windows&target_arch=x86_64&target_version=10&target_type=exe_local
