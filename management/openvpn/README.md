# Augmented Normalcy OpenVPN

> VPN Proxy for remote Augmented Normalcy access

NOTE: no pka is saved in this repository; only the configuration files

ALSO NOTE: This repo is located on the EC2 at /etc/openvpn. The installation of easy-rsa is located ~/easy-rsa

FINAL NOTE: Server is running on `systemctl` at `openvpn-server@server`

## Required reading

OpenVPN is pretty simple, but intimidating if you've never used it. To get familiar, use the book `Mastering OpenVPN, book by Eric F Crist`

## Installing the server 

For this installation, the tutorial `https://kifarunix.com/install-and-setup-openvpn-server-on-ubuntu-20-04/` was used, with small
changes based on the specific use case. There's a lot of material here, but if you have read the above mentioned book, it should
all make sense.

You shouldn't need to install anything since the EC2 instance is already built. Common tasks are documented below.

## Creating a new client

A new client is a user at that wants to connect to remote assets.

1. Follow directions from the above link in installing the server for generating OpenVPN client certificates and installing them
in the client directory. Make sure the client name is unique
   
2. In the client directory, follow the in place naming scheme. CLIENT is reserved for the on server client configuration,
CLIENT.d is the directory that contains the client files
   
3. Create a new client server configuration by copying generic-single-configuration to CLIENT. Make sure to change UNIQUE_NUMBER
to a unique IP address; you can see one's being used by checkin the other client server configurations, and the
server IP from server.conf

## downloading a key

scp -i "aws_ec2_keypair.pem" -r ubuntu@augmented-openvpn.polis.tv:/etc/openvpn/client client

   
### Windows

4. Install OpenVPN client from here `https://wdx-lims.s3.amazonaws.com/assets/OpenVPN-2.5.1-I601-amd64.msi`

5. Rename your tun, tap adapters to OpenVPN-Wintun, OpenVPN-Wintap respectively. You can acheive this by searching for
"Network Connections" and updating the names of Tap-Windows Adapter and Wintun Userspace Tunnel interfaces that were
installed by the OpenVPN client

6. Copy the generic-windows.d/client.ovpn to CLIENT.d/CLIENT.ovpn... replace client with the name of the client

7. Open the CLIENT.ovpn and replace the name of cert and key with the names used in step 1

8. Copy this configuration from the server onto the client at C:/ProgramFiles/config. There should be:
    - CLIENT.crt
    - CLIENT.key
    - CLIENT.ovpn
    - ca.crt
    - ta.key
    
9. From here, start up openvpn by searching for openvpn.

10. Connect by going to the menu at the bottom right of windows, finding the openVPN application, right clicking, and connecting.
This may take a minute...
   
11. Similarly, quit by right clicking, disconnecting, and then right clicking and hitting exit

12. https://strongvpn.com/autoconnect-windows-10-openvpn/

### Linux

4. Install OpenVPN client by `sudo apt-get update`, `sudo apt-get upgrade` and `sudo apt-get install openvpn`

5. Copy the generic-windows.d/client.ovpn to CLIENT.d/CLIENT.ovpn... replace client with the name of the client

6. Open the CLIENT.ovpn and replace the name of cert and key with the names used in step 1

7. Copy this configuration from the server onto the client at /etc/openvpn/client/. There should be:
    - CLIENT.crt
    - CLIENT.key
    - CLIENT.conf
    - ca.crt
    - ta.key
    
8. Point OpenVPN at the new client by running `sudo openvpn --config /etc/openvpn/client/CLIENT.conf`

9. Start OpenVPN by running `sudo systemctl start openvpn-client@CLIENT`
   
10. Stop OpenVPN by running `sudo systemctl stop openvpn`

## Creating a new machine

If you are trying to create a new config for a deployed machine:

### Linux

1. Follow instructions for a linux client, stop at 7.

2. Make Openvpn start at startup by running `sudo systemctl enable openvpn-client@CLIENT`

## Creating a new network proxy

If you need access to multiple computers on a subnet, for instance when 5 remote systems are deployed, all hooked up
to the same router, you only need to install OpenVPN on one of them, then use the NIC to proxy access to the other
machines.

### Linux

1. Follow creating a new machine, except instead of copying generic-single-configuration, use generic-proxy-configuration

2. Change LOCAL_IP_SUBNET to the IP subnet on premise; for instance, see bdl

3. Let the server know this subnet is available: add the lines `route LOCAL_IP_SUBNET 255.255.255.0 20.0.0.1` and
   `push "route LOCAL_IP_SUBNET 255.255.255.0"`. This lets clients know they can reach LOCAL_IP_SUBNET from the server.
   
** This next part is super invovled. We need to tell the NIC that it can forward traffic from 20.0.0.0 to the local subnet,
and make it look like its local traffic to the local router. Directions were taken from https://arashmilani.com/post?id=53,
https://unix.stackexchange.com/questions/125833/why-isnt-the-iptables-persistent-service-saving-my-changes, and the previously
mentioned book

4. Install some things: `sudo apt-get install iptables-persistent netfilter-persistent`

5. Forward traffic back and forth from tun to the network interface. In the case of bdl, the OpenVPN server was hooked up
through ethernet, and the device was named eth0. To find which adapter is in use, use `sudo ipconfig`. Anyways, the sauce
is

> sudo iptables -A FORWARD -i tun+ -j ACCEPT
> sudo iptables -A FORWARD -i tun+ -o eth0 -m state --state RELATED,ESTABLISHED -j ACCEPT
> sudo iptables -A FORWARD -i eth0 -o tun+ -m state --state RELATED,ESTABLISHED -j ACCEPT

again, you will likely need to change eth0... probably won't need to change tun+

6. To the newtwork interface to mask traffic from tun to the local subnet:

> sudo iptables -t nat -A POSTROUTING -s 20.0.0.0/24 -o eth0 -j MASQUERADE

7. Save the new configuration `sudo service netfilter-persistent save`

8. Edit /etc/sysctl.conf, uncomment `net.ipv4.ip_forward=1`; apply this change by running `sudo sysctl --system`

9. Finally, allow openVPN traffic through the firewall with `sudo ufw allow 1194/udp`

10. Reboot and pray :D



