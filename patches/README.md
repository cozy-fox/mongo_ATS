## To apply the plugin patch

This is an example of how to apply a patch;

```
$ wget -nv "http://downloads.asterisk.org/pub/telephony/asterisk/releases/asterisk-x" -O - | tar -zxf -
$ cd asterisk-x
$ wget https://github.com/minoruta/ast_mongo/blob/master/asterisk/patches/ast_mongo-x.patch
$ patch -p1 -i ast_mongo-x.patch
$ ./bootstrap.sh
$ ./configure
$ make
$ sudo make install
$ ...
```

## Regarding file name of the patches

Each file have a suffix based on an asterisk you specify in [`config.json`](../../config.json).
The minor revision of the suffix however is almost meaningless.
So you can use a patch which has same major version, or run [`./test`](../test_bench#test) to get a patch which has same version number.
