#!/usr/bin/env bash
#
# Provision MongoDB Backend Service
#
# Usage:  provision.sh $1
# Where:
#       $1 = version of MongoDB server
#
# Copyright: (C) 2016-17 KINOSHITA minoru
# License: The MIT License (MIT)
#

HOME='/home/vagrant'
VERSION_MONGODB=$1

#
# Install essential packages
#
sudo apt-get install -y \
    avahi-daemon
    
#
#   Install Mongodb             https://docs.mongodb.org/manual/tutorial/install-mongodb-on-ubuntu/
#
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 0C49F3730359A14518585931BC711F9BA15703C6
echo "deb [ arch=amd64 ] http://repo.mongodb.org/apt/ubuntu xenial/mongodb-org/$VERSION_MONGODB multiverse" | sudo tee /etc/apt/sources.list.d/mongodb-org-$VERSION_MONGODB.list
sudo apt-get update
sudo apt-get install -y mongodb-org

# change bind
sudo sed -i.bak -e s/127.0.0.1/0.0.0.0/g /etc/mongod.conf 
# restart mongod
sudo service mongod restart
# load test records
sleep 5
mongo < /vagrant/mongodb/testdata.script

#
#   end of provisioning
#
echo "==================================="
echo "`mongo --version` installed"
