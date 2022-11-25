# FastBinaryEncoding

Installation instructions at https://chronoxor.github.io/FastBinaryEncoding

After building, you should get FastBinaryEncoding/bin/fbec. Put this somewhere that's in your path (/usr/local/bin).
You're also going to need FastBinaryEncoding/bin/libproto.a; put it (usr/local/lib or something, idc

Compile a model like...
```
$ fbec --cpp --input=proto.fbe --output=.
```

Go ahead and run create_example.cpp by

```
$ g++ -std=c++17 -lproto -I. fbe.cpp -luuid fbe_models.cpp proto.cpp proto_models.cpp create.cpp -o create.run
```