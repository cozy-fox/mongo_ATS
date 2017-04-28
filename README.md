# MongoDB Plugins for Asterisk

The **`ast_mongo`** project provides the following plugins for Asterisk;

1. Realtime configuration engine with MongoDB,
1. CDR backend for MongoDB,
1. CEL backend for MongoDB (contributed by [viktike][9], thanks [viktike][9]),
1. and a [test bench](test_bench) with MongoDB replica set based on Docker technology.

Plugin name            |Realtime    |CDR|CEL|Source code|Config file(s)
-----------------------|------------|---|---|-----------|--------------
`res_mongodb.so`       | * | * | * | [`res_mongodb.c`](src/res_mongodb.c) |
`res_config_mongodb.so`| * |   |   | [`res_config_mongodb.c`](src/res_config_mongodb.c) | [`res_config_mongodb.conf`](test_bench/configs/res_config_mongodb.conf)<br>[`sorcery.conf`](test_bench/configs/sorcery.conf)<br>[`extconfig.conf`](test_bench/configs/extconfig.conf) | as realtime configuration engine
`cdr_mongodb.so`       |   | * |   | [`cdr_mongodb.c`](src/cdr_mongodb.c)|[`cdr_mongodb.conf`](test_bench/configs/cdr_mongodb.conf)
`cel_mongodb.so`       |   |   | * | [`cel_mongodb.c`](src/cel_mongodb.c)|[`cel_mongodb.conf`](test_bench/configs/cel_mongodb.conf)


## How to get the plugins

The plugins are provided as source code patches to Asterisk.
See [patches](patches) in detail.


## Test bench

The test bench based on Docker technology for these plugins is also provided.
You can examine how it works on your desktop simply.
See [test bench](test_bench) in detail.


## Setting up

### Preconditions (example)

- URI to database
  - `mongodb://mongodb.local/[name of database]`
  - see [Connection String URI Format](https://docs.mongodb.com/manual/reference/connection-string/) as well

- Database structure

Name of DB |Name of Collection |Comment
-----------|-------------------|-------
`asterisk` | `ps_endpoints`    | as realtime resources
`asterisk` | `ps_auths`        | as realtime resources
`asterisk` | `ps_aors`         | as realtime resources
`asterisk` | `ast_config`      | as non-realtime resources
`cdr`      | `cdr`
`cel`      | `cel`

### Config files for the exmaple


- [`res_config_mongodb.conf`](test_bench/configs/res_config_mongodb.conf) for realtime configuration engine

        [mongodb]
        uri=mongodb://mongodb.local/asterisk    ; location of database

- [`sorcery.conf`](test_bench/configs/sorcery.conf) specifies map from asterisk's resources to database's collections.

        [res_pjsip]
        endpoint=realtime,ps_endpoints  ; map endpoint to ps_endpoints source
        auth=realtime,ps_auths          ; map auth to ps_auths source
        aor=realtime,ps_aors            ; map aor to ps_aors source

- [`extconfig.conf`](test_bench/configs/extconfig.conf) specifies database for database's collections mapped above.

        [settings]

        ; specify the ps_endpoints source is in asterisk database provided by ast_mongo plugin
        ; i.e. endpoint => ps_endpoints => asterisk database of mongodb plugin
        ps_endpoints => mongodb,asterisk
        ps_auths => mongodb,asterisk
        ps_aors => mongodb,asterisk

        ; map extensions.conf to ast_config collection of asterisk database
        extensions.conf => mongodb,asterisk,ast_config 
        pjsip.conf => mongodb,asterisk,ast_config

- [`cdr_mongodb.conf`](test_bench/configs/cdr_mongodb.conf) specifies the location, name and collection of database for cdr backend.

        [mongodb]
        uri=mongodb://mongodb.local/cdr ; location of database
        database=cdr                    ; name of database
        collection=cdr                  ; name of collection to record cdr data

- [`cel_mongodb.conf`](test_bench/configs/cel_mongodb.conf) specifies the location, name and collection of database for cel backend.

        [mongodb]
        uri=mongodb://mongodb.local/cel ; location of database
        database=cel                    ; name of database
        collection=cel                  ; name of collection to record cel data

- See Asterisk's official document [Setting up PJSIP Realtime][5] as well.

## License and Copyright

- The related code to Asterisk: 
    - GNU GENERAL PUBLIC LICENSE Version 2
- Any other resources and files: 
    - The MIT License (MIT)
- Copyright: (C) 2016-17, KINOSHITA minoru, [viktike][9] for cel_mongodb


[1]: http://asterisk.org/        "Asterisk"
[2]: https://mongodb.org/        "MongoDB"
[3]: https://github.com/mongodb/mongo-c-driver  "mongo_c_driver"
[4]: http://www.pjsip.org       "PJSIP"
[5]: https://wiki.asterisk.org/wiki/display/AST/Setting+up+PJSIP+Realtime
[6]: https://github.com/minoruta/ast_mongo/tree/master/asterisk
[9]: https://github.com/viktike
