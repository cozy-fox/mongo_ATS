#!/usr/bin/env bash
#
# Provision MongoDB Backend Service
#
# Usage:  provision.sh $1
# Where:
#       $1 = version of MongoDB server
#
# Copyright: (C) 2016 KINOSHITA minoru
# License: The MIT License (MIT)
#

VERSION_MONGODB=$1

#
#   Update system
#
sudo apt-get update
sudo apt-get upgrade -y

#
# Install essential packages
#
sudo apt-get install -y \
    avahi-daemon
    
#
#   Install Mongodb             https://docs.mongodb.org/manual/tutorial/install-mongodb-on-ubuntu/
#
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv EA312927
echo "deb http://repo.mongodb.org/apt/ubuntu trusty/mongodb-org/$VERSION_MONGODB multiverse" | sudo tee "/etc/apt/sources.list.d/mongodb-org-$VERSION_MONGODB.list"
sudo apt-get update
sudo apt-get install -y --force-yes mongodb-org

# load test records
mongo < /vagrant/mongodb/testdata.script
# change bind
sudo sed -i.bak -e s/127.0.0.1/0.0.0.0/g /etc/mongod.conf 
# restart mongod
sudo service mongod restart

#
#   end of provisioning
#
echo "==================================="
echo "`mongo --version` installed"
