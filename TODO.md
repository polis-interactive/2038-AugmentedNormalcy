
# NTP
- flash new conf without min max poll
- Hard reset ntp on all pis during boot

# HEADSETS
- Enable uart on all headsets / dtoverlay=disable-bt (copied to system dir)
   https://di-marco.net/blog/it/2020-04-18-tips-disabling_bluetooth_on_raspberry_pi/
- Install pigpio 
- Need to add  echo "kernel.sysrq = 0" | sudo tee -a /etc/sysctl.conf when disable-bt so the tty port doesn’t complain (copied to system dir)

# Camera
- Need to add dtoverlay=imx708,rotation=0 to rotate cameras (copied to system dir)
- Need to add config file for each ip / update camera.service to pass the ip as config option

# Problems
- Why when a camera disconnects is it having trouble reconnecting? its like the server
   holds a reference to it or something
- Headset 104 is not working; might work on it