# Module Funtion Tests

[This VM][4] provides testing environment for plugins;

1. res_config_mongodb-test for realtime configuration engine,
2. cdr_mongodb-test for CDR backend,
3. cel_mongodb-test for CEL backend.

## Launch a VM for testing

    desktop$ cd nodejs                  ; select nodejs VM
    desktop$ vagrant up                 ; launch it and generate environment to test plugins.


## Test plugins

1. Launch the [mongodb VM](../mongodb), and [reload the initial test records][2] if you need.
2. Launch the [asterisk VM](../asterisk), and make sure `/etc/asterisk/*.conf` files keep initial conditions.
3. Launch the nodejs VM (this VM), then execute `npm test` as follows;

```
desktop$ cd nodejs
desktop$ vagrant up
desktop$ vagrant ssh
Welcome to Ubuntu 14.04.4 LTS (GNU/Linux 3.19.0-25-generic x86_64)
...

vagrant@nodejs:~$ cd nodejs/res_config_mongodb/
vagrant@nodejs:~/nodejs/res_config_mongodb$ npm test

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

  CEL by cel_mongodb
    ✓ should log event transactions (88ms)

  1 passing (1s)

vagrant@nodejs:~/nodejs/res_config_mongodb$ 
```
## Test conditions

[`config.json`][3] collects test conditions for this project.

```json
{
    "asterisk": {
        "host": "asterisk.local",
        "uri": "ws://asterisk.local:8088/ws"
    },
    "ari": {
        "uri": "http://asterisk.local:8088",
        "account": {
            "user": "asterisk",
            "password": "asterisk"
        }
    },
    "mongodb": {
        "config": {
            "uri": "mongodb://mongodb.local:27017",
            "db": "asterisk"
        },
        "cel": {
            "uri": "mongodb://mongodb.local:27017",
            "db": "cel",
            "collection": "cel"
        },
        "cdr": {
            "uri": "mongodb://mongodb.local:27017",
            "db": "cdr",
            "collection": "cdr"
        }
    },
    "sip": {
        "stunServers": [],
        "registerExpires": 3,
        "traceSip": false,
        "log": {
            "level": 0
        }
    }
}
```

## Vagrantfile

The following property can be configurable with a common file [`config.json`](../config.json);

| Property  | Definition           | Defined |
|-----------|----------------------|---------|
| `nodejs`  |version of the nodejs | '4.x'   |

## Debug

Set `sip.traceSip` and `sip.log.level` with `true` and `3` of `config.json` repectively. 
See [SIP.UA Configuration Parameters][1] for detail.

    {
        ...
        "sip": {
            ...
            "traceSip": true,
            "log": {
                "level": 3
            }
        }
    }

## License and Copyright

- License: The MIT License (MIT)
- Copyright: (C) 2016, KINOSHITA minoru

[1]: http://sipjs.com/api/0.7.0/ua_configuration_parameters/
[2]: ../mongodb#reload-initial-test-records
[3]: nodejs/res_config_mongodb/config.json
[4]: https://github.com/minoruta/ast_mongo/tree/master/nodejs
