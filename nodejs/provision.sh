#!/usr/bin/env bash
#
# Provision applications to test with Nodejs
# Usage:  provision.sh
#
# Copyright: (C) 2016 KINOSHITA minoru
# License: The MIT License (MIT)
#

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
    libkrb5-dev \
    curl \
    vim

#
#   Install Nodejs              https://nodejs.org/en/download/package-manager/
#
curl -sL https://deb.nodesource.com/setup_4.x | sudo -E bash -
sudo apt-get install -y nodejs
echo "node version `node --version` installed"
echo "npm version `npm --version` installed"

#
#   Install test applications
#
ln -s /vagrant/nodejs
cd nodejs/res_config_mongodb
npm install
echo "test application installed"
