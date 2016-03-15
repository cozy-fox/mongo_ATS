# VM for MongoDB

The [`VM for MongoDB`][1] provides database service for this development environment.

## Launch a VM as backend database service

    desktop$ cd mongodb
    desktop$ vagrant up


## Reload initial test records

You can reload the initial test records if you need,
although it is loaded automatically while the VM has been launched up at first time as above.

    desktop$ cd mongodb/
    desktop$ vagrant ssh
    Welcome to Ubuntu 14.04.4 LTS (GNU/Linux 3.19.0-25-generic x86_64)
    ...

    vagrant@mongodb:~$ mongo < /vagrant/mongodb/testdata.script 
    MongoDB shell version: 3.2.3
    ...
    bye
    vagrant@mongodb:~$ 


## Vagrantfile

The following property can be configurable with a common file [`config.json`](../config.json);

| Property  | Definition           | Defined |
|-----------|----------------------|---------|
| `mongodb` |version of the mongodb| '3.2'   |


## License and Copyright

- License: The MIT License (MIT)
- Copyright: (C) 2016, KINOSHITA minoru

[1]: https://github.com/minoruta/ast_mongo/tree/master/mongodb
