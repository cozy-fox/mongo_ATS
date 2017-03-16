# Realtime configuration engine and CDR&CEL backend for Asterisk with MongoDB

The **`ast_mongo`** project provides the following plugins for Asterisk;

1. Realtime configuration engine with MongoDB,
1. CDR backend for MongoDB,
1. CEL backend for MongoDB (contributed by [viktike][9], thanks [viktike][9]),
1. and a [test bench](asterisk/test_bench) with MongoDB replica set based on Docker technology.

#### See [MongoDB Plugins for Asterisk](asterisk) in detail.

---
#### Note: Another test bench based on Vagrant was deprecated. See Old [README.VAGRANT](README.VAGRANT.md).

[1]: http://asterisk.org/        "Asterisk"
[2]: https://mongodb.org/        "MongoDB"
[3]: https://github.com/mitchellh/vagrant   "Vagrant"
[4]: https://github.com/Parallels/vagrant-parallels     "vagrant-parallels"
[5]: https://www.parallels.com  "Parallels Desktop for Mac"
[6]: https://www.vagrantup.com/docs/vagrantfile/    "Vagrantfile"
[7]: https://atlas.hashicorp.com/parallels/boxes/ubuntu-16.04
[8]: https://github.com/minoruta/ast_mongo
[9]: https://github.com/viktike

---
## License and Copyright

- The related code to Asterisk: 
    - GNU GENERAL PUBLIC LICENSE Version 2
- Any other resources and files: 
    - The MIT License (MIT)
- Copyright: (C) 2016-17, KINOSHITA minoru, [viktike][9] for cel_mongodb
