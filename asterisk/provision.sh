#!/usr/bin/env bash
#
# Provision Asterisk with the essential environments
#
# Usage:  provision.sh $1 $2 $3 $4
# Where:
#       $1 = version of MongoDB Server
#       $2 = version of MongoDB C Driver
#       $3 = version of pjsip
#       $4 = version of Asterisk
#
# Copyright: (C) 2016 KINOSHITA minoru
# License: The MIT License (MIT)
#

VERSION_MONGODB=$1
VERSION_MONGOC=$2
VERSION_PJSIP=$3
VERSION_ASTERISK=$4

#
#   Update system
#
sudo apt-get update
sudo apt-get upgrade -y

#
# Install essential packages
#
sudo apt-get install -y \
    avahi-daemon \
    build-essential \
    pkg-config \
    autoconf \
    libcurl4-openssl-dev \
    libsrtp0-dev \
    libssl-dev \
    libncurses5-dev \
    libnewt-dev \
    libxml2-dev \
    libsqlite3-dev \
    libjansson-dev \
    uuid-dev \
    wget \
    git \
    vim \
    gdb

#
#   Install the latest MongoDB Tools for client        https://www.mongodb.org/downloads
#
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv EA312927
echo "deb http://repo.mongodb.org/apt/ubuntu trusty/mongodb-org/$VERSION_MONGODB multiverse" | sudo tee "/etc/apt/sources.list.d/mongodb-org-$VERSION_MONGODB.list"
sudo apt-get update
sudo apt-get install -y mongodb-org-shell mongodb-org-tools

#
#   Install MongoDB C Driver for client     https://github.com/mongodb/mongo-c-driver/releases
#
cd /home/vagrant
wget -nv "https://github.com/mongodb/mongo-c-driver/releases/download/$VERSION_MONGOC/mongo-c-driver-$VERSION_MONGOC.tar.gz" -O - | tar xzf -
cd mongo-c-driver-$VERSION_MONGOC
./configure
make
sudo make install

#
#   Install pjsip library                   http://www.pjsip.org/download.htm
#
cd /home/vagrant
wget -nv "http://www.pjsip.org/release/$VERSION_PJSIP/pjproject-$VERSION_PJSIP.tar.bz2" -O - | tar xjf -
cd pjproject-$VERSION_PJSIP
git init
git add .
git commit . -m "initial"
./configure \
    --enable-shared \
    --disable-oss \
    --disable-video \
    --disable-speex-aec \
    --disable-g722-codec \
    --disable-g7221-codec \
    --disable-speex-codec \
    --disable-ilbc-codec \
    --disable-sdl \
    --disable-v4l2 \
    --disable-opencore-amr \
    --disable-silk
make dep && make
sudo make install
sudo ldconfig

#
#   Build and Install Asterisk              http://www.asterisk.org/downloads
#
cd /home/vagrant
wget -nv "http://downloads.asterisk.org/pub/telephony/asterisk/releases/asterisk-$VERSION_ASTERISK.tar.gz" -O - | tar -zxf -
cd asterisk-$VERSION_ASTERISK
git init
git add .
git commit . -m "initial"
## add files for MongoDB functions
cd ./cdr
ln -s /vagrant/src/cdr_mongodb.c
cd ../res
ln -s /vagrant/src/res_mongodb.c
ln -s /vagrant/src/res_mongodb.exports.in
ln -s /vagrant/src/res_config_mongodb.c
cd ../include/asterisk
ln -s /vagrant/src/res_mongodb.h
cd ../../
##  patch for configure.ac makeopts.in build_tools/menuselect-deps.in
##  which was generated with 'git diff configure.ac makeopts.in build_tools/menuselect-deps.in'
patch -p1 -i /vagrant/src/mongodb.for.asterisk.patch
## reconfigure
./bootstrap.sh
./configure
make menuselect.makeopts
menuselect/menuselect \
    --disable cdr_csv \
    --disable cdr_sqlite3_custom \
    menuselect.makeopts
# build
make
sudo make install
sudo ldconfig
# make it as daemon
sudo make config
## set up asterisk for test environment
cd /etc/asterisk
sudo ln -s /vagrant/configs/asterisk.conf
sudo ln -s /vagrant/configs/modules.conf
sudo ln -s /vagrant/configs/logger.conf
sudo ln -s /vagrant/configs/sorcery.conf
sudo ln -s /vagrant/configs/extconfig.conf
sudo ln -s /vagrant/configs/cdr_mongodb.conf 
sudo ln -s /vagrant/configs/res_config_mongodb.conf
sudo ln -s /vagrant/configs/cdr.conf
sudo ln -s /vagrant/configs/rtp.conf
sudo ln -s /vagrant/configs/http.conf
sudo ln -s /vagrant/configs/ari.conf
sudo ln -s /vagrant/configs/pjsip.conf

#
#
#
sudo service asterisk start
