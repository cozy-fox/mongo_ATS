ARG VERSION_UBUNTU=latest
FROM ubuntu:$VERSION_UBUNTU
MAINTAINER KINOSHITA minoru <5021543+minoruta@users.noreply.github.com>

#
#   Essential arguments
#
ARG VERSION_ASTERISK
ARG VERSION_MONGOC
ARG VERSION_LIBSRTP

WORKDIR /root
RUN  mkdir src
COPY src/* src/
RUN  mkdir ast_mongo

RUN apt -qq update \
&&  apt -qq install -y \
    libssl-dev \
    libsasl2-dev \
    libncurses5-dev \
    libnewt-dev \
    libxml2-dev \
    libsqlite3-dev \
    libjansson-dev \
    libcurl4-openssl-dev \
    pkg-config \
    build-essential \
    autoconf \
    uuid-dev \
    wget \
    file \
    git

#
#   Prepare strp
#
WORKDIR /root
RUN wget https://github.com/cisco/libsrtp/archive/v$VERSION_LIBSRTP.tar.gz \
&&  tar xzf v$VERSION_LIBSRTP.tar.gz \
&&  rm v$VERSION_LIBSRTP.tar.gz \
&&  cd libsrtp-$VERSION_LIBSRTP \
&&  ./configure > /dev/null \
&&  make \
&&  make install \
&&  make clean \
&&  ldconfig \
&&  cd ..

#
#   Prepare MongoDB C Driver
#
RUN cd $HOME \
&&  wget -nv "https://github.com/mongodb/mongo-c-driver/releases/download/$VERSION_MONGOC/mongo-c-driver-$VERSION_MONGOC.tar.gz" -O - | tar xzf - \
&&  cd mongo-c-driver-$VERSION_MONGOC \
&&  ./configure --disable-automatic-init-and-cleanup > /dev/null \
&&  make all install > make.log \
&&  make clean \
&&  cd $HOME \
&&  tar czf mongo-c-driver-$VERSION_MONGOC.tgz mongo-c-driver-$VERSION_MONGOC \
&&  rm -rf mongo-c-driver-$VERSION_MONGOC

#
#   Build and install Asterisk with patches for ast_mongo
#
RUN cd $HOME \
&&  wget -nv "http://downloads.asterisk.org/pub/telephony/asterisk/releases/asterisk-$VERSION_ASTERISK.tar.gz" -O - | tar -zxf - \
&&  cd asterisk-$VERSION_ASTERISK \
&&  git config --global user.email "you@example.com" \
&&  git init &&  git add . &&  git commit . -m "initial" \
&&  cd $HOME/asterisk-$VERSION_ASTERISK/cdr \
&&  cp $HOME/src/cdr_mongodb.c . \
&&  git add . \
&&  cd $HOME/asterisk-$VERSION_ASTERISK/cel \
&&  cp $HOME/src/cel_mongodb.c . \
&&  git add . \
&&  cd $HOME/asterisk-$VERSION_ASTERISK/res \
&&  cp $HOME/src/res_mongodb.c . \
&&  cp $HOME/src/res_mongodb.exports.in . \
&&  cp $HOME/src/res_config_mongodb.c . \
&&  git add . \
&&  cd $HOME/asterisk-$VERSION_ASTERISK/include/asterisk \
&&  cp $HOME/src/res_mongodb.h . \
&&  git add . \
&&  cd $HOME/asterisk-$VERSION_ASTERISK \
&&  patch -p1 -F3 -i $HOME/src/mongodb.for.asterisk.patch \
&&  git diff build_tools/menuselect-deps.in configure.ac makeopts.in > $HOME/ast_mongo/mongodb.for.asterisk.patch \
&&  git diff HEAD > $HOME/ast_mongo/ast_mongo-$VERSION_ASTERISK.patch \
&&  ./bootstrap.sh \
&&  ./configure --disable-xmldoc > /dev/null \
&&  tar czf $HOME/ast_mongo/asterisk-$VERSION_ASTERISK-config.log.tgz config.log \
&&  make all > make.log \
&&  make install > install.log \
&&  ldconfig /usr/lib \
&&  make samples > samples.log \
&&  make clean > clean.log

#
# Copy back the updated patches to host & Launch asterisk
#
CMD cp /root/ast_mongo/ast_mongo-* /mnt/ast_mongo/patches/ \
&&  cp /root/ast_mongo/mongodb.for.asterisk.patch /mnt/ast_mongo/src/ \
&&  asterisk -c > /dev/null
