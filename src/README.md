## Building ast_mongo plugins

In order to build the `ast_mongo` plugins, you need to build it with Asterisk.

1. Prepare environment to build asterisk. See [Checking Asterisk Requirements](https://wiki.asterisk.org/wiki/display/AST/Checking+Asterisk+Requirements) in detail.
1. Prepare [MongoDB C Driver](https://github.com/mongodb/mongo-c-driver);
 ```
 $ wget -nv "https://github.com/mongodb/mongo-c-driver/releases/download/$VERSION_MONGOC/mongo-c-driver-$VERSION_MONGOC.tar.gz" -O - | tar xzf -
 $ cd mongo-c-driver-$VERSION_MONGOC
 $ mkdir cmake-build
 $ cd cmake-build
 $ cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF ..
 $ make all && sudo make install
 ```

1. Prepare the code of asterisk;
 ```
 $ wget -nv "http://downloads.asterisk.org/pub/telephony/asterisk/releases/asterisk-a.b.c.tar.gz" -O - | tar -zxf -
 $ cd asterisk-a.b.c
 ```
1. Copy following files to `dst` of asterisk's source tree respectively;

files                  | dst
-----------------------|---------
cdr_mongodb.c          | ./cdr/
cel_mongodb.c          | ./cel/
res_config_mongodb.c   | ./res/
res_mongodb.c          | ./res/
res_mongodb.exports.in | ./res/
res_mongodb.h          | ./include/asterisk/

5. Apply `mongodb.for.asterisk.patch`;
 ```
 $ patch -p1 -F3 -i mongodb.for.asterisk.patch
 ```
1. Update `configure`;
 ```
 $ ./bootstrap.sh
 ```
1. Buld Asterisk. See [Building and Installing Asterisk](https://wiki.asterisk.org/wiki/display/AST/Building+and+Installing+Asterisk) as well.
 ```
 $ ./configure ...
 $ make
 $ sudo make install
 ```
