#!/usr/bin/env bash
#
# Provision Asterisk with the essential environments
#
# Usage:  provision.sh $1 $2 $3
# Where:
#       $1 = version of MongoDB C Driver
#       $1 = version of pjsip
#       $2 = version of Asterisk
#
# Copyright: (C) 2016-17 KINOSHITA minoru
# License: The MIT License (MIT)
#
VERSION_MONGOC=$1
VERSION_PJSIP=$2
VERSION_ASTERISK=$3
MOUNT_POINT=/mnt
if [ $EUID -ne 0 ]; then
    SUDO=sudo
else
    SUDO=''
fi
#
#
#
git config --global user.email "you@example.com"

#
#   Install MongoDB C Driver for client     https://github.com/mongodb/mongo-c-driver/releases
#
cd $HOME
wget -nv "https://github.com/mongodb/mongo-c-driver/releases/download/$VERSION_MONGOC/mongo-c-driver-$VERSION_MONGOC.tar.gz" -O - | tar xzf -
cd mongo-c-driver-$VERSION_MONGOC
./configure --disable-automatic-init-and-cleanup
make
$SUDO make install

#
#   Install pjsip library                   http://www.pjsip.org/download.htm
#
if [ "$VERSION_PJSIP" != "bundled" ] ; then
    cd $HOME
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
    $SUDO make install
    $SUDO ldconfig
else
    PJBUNDLED="--with-pjproject-bundled"
fi

#
#   Build and Install Asterisk              http://www.asterisk.org/downloads
#
cd $HOME
wget -nv "http://downloads.asterisk.org/pub/telephony/asterisk/releases/asterisk-$VERSION_ASTERISK.tar.gz" -O - | tar -zxf -
cd $HOME/asterisk-$VERSION_ASTERISK
git init
git add .
git commit . -m "initial"
## add files for MongoDB functions

if true ; then
    if false ; then
        cd $HOME/asterisk-$VERSION_ASTERISK/cdr
        ln -s $MOUNT_POINT/src/cdr_mongodb.c
        cd $HOME/asterisk-$VERSION_ASTERISK/cel
        ln -s $MOUNT_POINT/src/cel_mongodb.c
        cd $HOME/asterisk-$VERSION_ASTERISK/res
        ln -s $MOUNT_POINT/src/res_mongodb.c
        ln -s $MOUNT_POINT/src/res_mongodb.exports.in
        ln -s $MOUNT_POINT/src/res_config_mongodb.c
        cd $HOME/asterisk-$VERSION_ASTERISK/include/asterisk
        ln -s $MOUNT_POINT/src/res_mongodb.h
        cd $HOME/asterisk-$VERSION_ASTERISK
        ##  patch for configure.ac makeopts.in build_tools/menuselect-deps.in
        ##  which was generated with 'git diff configure.ac makeopts.in build_tools/menuselect-deps.in'
        patch -p1 -F3 -i $MOUNT_POINT/src/mongodb.for.asterisk.patch
    else
        cd $HOME/asterisk-$VERSION_ASTERISK/cdr
        cp $MOUNT_POINT/src/cdr_mongodb.c .
        git add .
        cd $HOME/asterisk-$VERSION_ASTERISK/cel
        cp $MOUNT_POINT/src/cel_mongodb.c .
        git add .
        cd $HOME/asterisk-$VERSION_ASTERISK/res
        cp $MOUNT_POINT/src/res_mongodb.c .
        cp $MOUNT_POINT/src/res_mongodb.exports.in .
        cp $MOUNT_POINT/src/res_config_mongodb.c .
        git add .
        cd $HOME/asterisk-$VERSION_ASTERISK/include/asterisk
        cp $MOUNT_POINT/src/res_mongodb.h .
        git add .
        cd $HOME/asterisk-$VERSION_ASTERISK
        ##  patch for configure.ac makeopts.in build_tools/menuselect-deps.in
        ##  which was generated with 'git diff configure.ac makeopts.in build_tools/menuselect-deps.in'
        patch -p1 -F3 -i $MOUNT_POINT/src/mongodb.for.asterisk.patch
        mkdir -p $MOUNT_POINT/patches
        git diff HEAD > $MOUNT_POINT/patches/ast_mongo-$VERSION_ASTERISK.patch
    fi
else
    patch -p1 -F3 -i $MOUNT_POINT/patches/ast_mongo-$VERSION_ASTERISK.patch
fi

## reconfigure
./bootstrap.sh
./configure $PJBUNDLED
make menuselect.makeopts
menuselect/menuselect \
    --disable cdr_csv \
    --disable cdr_sqlite3_custom \
    menuselect.makeopts
# build
make
$SUDO make install
$SUDO ldconfig
# make it as daemon
$SUDO make config
## set up asterisk for test environment
for conf in \
    acl.conf \
    features.conf \
    pjproject.conf \
    pjsip_notify.conf \
    pjsip_wizard.conf \
    res_config_sqlite3.conf \
    res_parking.conf \
    statsd.conf \
    udptl.conf \
    calendar.conf
do
    $SUDO touch /etc/asterisk/$conf
done
for conf in \
    features.conf \
    indications.conf \
    res_stun_monitor.conf \
    phoneprov.conf \
    cli_aliases.conf \
    users.conf \
    ccss.conf \
    hep.conf
do
    $SUDO cp configs/samples/$conf.sample /etc/asterisk/$conf
done
#
#
#
$SUDO service asterisk start

#
#   end of provisioning
#
echo "==================================="
echo "`asterisk -V` installed"
echo "==================================="
