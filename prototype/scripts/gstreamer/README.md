
# GStreamer

Hopefully we can just use upstream / don't have to compile

## Ubuntu

### Setup

```
$ sudo apt-get update
$ sudo apt-get upgrade
$ sudo apt-get install -y apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-tools \
    gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-gtk3 \
    gstreamer1.0-qt5 gstreamer1.0-pulseaudio x264 libx264-dev
```

### Running

I have a shitty webcam on my dusty ass laptop. Change the resolution as necessary. MJPEG here is more performant as
on the h264 I need to reformat the video; in this case it doesn't matter as loopback bypasses the NIC. To get
h264 going I actually didn't have any luck with avenc_h264_omx, and for some reason my old ass laptop didn't like
x264enc, which is the standard... install the nvidia ones following these directions (loosely):

https://lifestyletransfer.com/how-to-install-nvidia-gstreamer-plugins-nvenc-nvdec-on-ubuntu/

```
# MJPEG Server
$ gst-launch-1.0 -e -vvv v4l2src ! 'video/x-raw,width=640,height=480,framerate=30/1' ! jpegenc ! rtpjpegpay pt=26 ! udpsink host=192.168.1.81 port=5000
# MJPEG Client
$ gst-launch-1.0 -e -vvv udpsrc port=5000 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! xvimagesink sync=false

# H264 Server (this was actually brutal)
$ gst-launch-1.0 -e -vvvv v4l2src device=/dev/video0 ! 'video/x-raw,width=640,height=480,framerate=30/1,format=YUY2' ! \
    videoconvert n-threads=2  ! nvh264enc ! rtph264pay ! udpsink host=192.168.1.81 port=5000
# H264 Client
$ gst-launch-1.0 -e -vvvv udpsrc port=5000 ! application/x-rtp, payload=96 ! rtph264depay ! nvdec ! glimagesink sync=false
```