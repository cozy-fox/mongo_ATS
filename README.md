# Configuration engine and CDR backend for Asterisk with MongoDB

[This project][8] provides the following plugins for [Asterisk][1] with [MongoDB][2];

1. MongoDB configuration engine,
2. MongoDB CDR backend,

and the development and test environment as well.

## Shortcut to launch asterisk with ast_mongo

You can use the [patches](asterisk/patches) to make asterisk with ast_mongo;

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

## Install, build and test it

This is another opiton to test ast_mongo;

1. `git clone https://github.com/minoruta/ast_mongo.git`
2. Launch the [mongodb VM](mongodb) to make backend MongoDB service.
3. Launch the [asterisk VM](asterisk) to build and run Asterisk server.
4. Launch the [nodejs VM](nodejs) to execute function tests.

```
desktop$ git clone https://github.com/minoruta/ast_mongo.git
desktop$ cd ast_mongo
desktop$ cd mongodb                     ; select mongodb VM
desktop$ vagrant up                     ; launch it as MongoDB server with test data
...
desktop$ cd ../asterisk                 ; select asterisk VM
desktop$ vagrant up                     ; launch it and build and run Asterisk
...
desktop$ cd ../nodejs                   ; select nodejs VM
desktop$ vagrant up                     ; launch it and prepare an environment to test
desktop$ vagrant ssh                    ; login it
Welcome to Ubuntu 14.04.4 LTS (GNU/Linux 3.19.0-25-generic x86_64)
...

vagrant@nodejs:~$ cd nodejs/res_config_mongodb/
vagrant@nodejs:~/nodejs/res_config_mongodb$ npm test            ; start the test

> functionTest@0.1.0 test /vagrant/nodejs/res_config_mongodb
> make

  A PJSIP UAS managed by res_config_mongodb
    ✓ should be offline at first (227ms)
    ✓ with unknown user should get 401 (227ms)
    ✓ with invalid password should get 401 (40ms)
    ✓ with right account should be online (109ms)
    ✓ with invalidated account should get 401 (73ms)
    ✓ should be offline at last (74ms)

  6 passing (2s)

  CDR by cdr_mongodb
    ✓ should log a call transaction (95ms)

  1 passing (1s)

vagrant@nodejs:~/nodejs/res_config_mongodb$ 
```

## VMs and the network

This project adds the three VMs in your local network;

|VM                  |Function                                        |Guest OS    |mDNS name     |
|--------------------|------------------------------------------------|------------|--------------|
|[mongodb](mongodb)  |provides backend MongoDB service                |Ubuntu 14.04|mongodb.local |
|[asterisk](asterisk)|provides an evironment to build and run Asterisk|Ubuntu 14.04|asterisk.local|
|[nodejs](nodejs)    |provides an evironment to test Asterisk         |Ubuntu 14.04|nodejs.local  |

## Requirements

This project is developed under the following environments;

- [Vagrant][3] 1.8.1
    - box: [parallels/ubuntu-14.04][7], v1.0.10
    - plugin: [vagrant-parallels][4] (1.6.1)
        - with: [Parallels Desktop for Mac][5], version 10.3
    - The [Vagrantfile][6]s of this project are written for vagrant-parallels.
- MacBook Pro, OS X El Capitan version 10.11, 2 cores, 8GB memory.


## Licenses and Copyright

- Licenses: 
    - The related programs for Asterisk: 
        - GNU GENERAL PUBLIC LICENSE Version 2
    - Any other resources and files: 
        - The MIT License (MIT)
- Copyright: (C) 2016, KINOSHITA minoru

[1]: http://asterisk.org/        "Asterisk"
[2]: https://mongodb.org/        "MongoDB"
[3]: https://github.com/mitchellh/vagrant   "Vagrant"
[4]: https://github.com/Parallels/vagrant-parallels     "vagrant-parallels"
[5]: https://www.parallels.com  "Parallels Desktop for Mac"
[6]: https://www.vagrantup.com/docs/vagrantfile/    "Vagrantfile"
[7]: https://atlas.hashicorp.com/parallels/boxes/ubuntu-14.04
[8]: https://github.com/minoruta/ast_mongo
