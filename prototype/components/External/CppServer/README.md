# CppServer

Installation instructions at https://github.com/chronoxor/CppServer#how-to-build

After building, you should get CppServer/bin/libcppserver.a. put it at usr/local/lib or something, idc. It should
also build libproto.a, but we already have that along with the FBE compiler (see ../FastBinary/Encoding.README.md)

Oh god, you also need libasio.a, libcppcommon.a, libfmt.a (he references the git fmt lib, so can't rely on system)


Go ahead and run udp_echo.cpp

```
$  g++ -std=c++17 -I./include -I../Asio/include -I../CppCommon/include udp_echo_client.cpp \
    -lcppserver -lproto -lasio -lcppcommon -luuid -lbfd -lcrypto -lssl -lfmt1 -o udp_echo_client.run
$  g++ -std=c++17 -I./include -I../Asio/include -I../CppCommon/include udp_echo_server.cpp \
    -lcppserver -lproto -lasio -lcppcommon -luuid -lbfd -lcrypto -lssl -lfmt1 -o udp_echo_server.run
```