#!/usr/bin/env bash
#
# Provision applications to test with Nodejs
# Usage:  provision.sh $1
# Where:
#       $1 = version of Nodejs
#
# Copyright: (C) 2016-17 KINOSHITA minoru
# License: The MIT License (MIT)
#

HOME='/home/vagrant'
VERSION_NODEJS=$1

#
# Install essential packages
#
sudo apt-get install -y \
    avahi-daemon \
    build-essential \
    libkrb5-dev \
    curl \
    vim

#
#   Install Nodejs              https://nodejs.org/en/download/package-manager/
#
curl -sL https://deb.nodesource.com/setup_$VERSION_NODEJS | sudo -E bash -
sudo apt-get install -y nodejs

#
#   Install test applications
#
ln -s /vagrant/nodejs
cd nodejs/res_config_mongodb
npm install
echo "test application installed"

#
#   end of provisioning
#
echo "==================================="
echo "node version `node --version` installed"
echo "npm version `npm --version` installed"
