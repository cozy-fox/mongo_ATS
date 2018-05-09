## To apply the plugin patch

This is an example of how to apply a patch;

```
$ wget -nv "http://downloads.asterisk.org/pub/telephony/asterisk/releases/asterisk-a.b.c" -O - | tar -zxf -
$ cd asterisk-a.b.c
$ wget https://raw.githubusercontent.com/minoruta/ast_mongo/master/patches/ast_mongo-x.y.z.patch
$ patch -p1 -i ast_mongo-x.y.z.patch
$ ./bootstrap.sh
$ ./configure
$ make
$ sudo make install
$ ...
```

You can get a specific version patch. See [`test/docker`](../test/docker) in detail.
