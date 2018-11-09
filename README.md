# MongoDB Plugins for Asterisk [![Build Status](https://travis-ci.org/minoruta/ast_mongo.svg?branch=master)](https://travis-ci.org/minoruta/ast_mongo)

The **`ast_mongo`** project provides the following plugins for Asterisk;

1. Realtime configuration engine with MongoDB,
1. Queue logging for MongoDB (preliminarly),
1. CDR backend for MongoDB,
1. CEL backend for MongoDB (contributed by [viktike][9], thanks [viktike][9]),
1. ~~and a [test bench](test_bench) with MongoDB replica set based on Docker technology~~(Deprecated).
1. and a [test bench](test/docker) based on Docker technology.

Plugin name            |Realtime    |CDR|CEL|Source code|Config file(s)
-----------------------|------------|---|---|-----------|--------------
`res_mongodb.so`       | * | * | * | [`res_mongodb.c`](src/res_mongodb.c) |
`res_config_mongodb.so`| * |   |   | [`res_config_mongodb.c`](src/res_config_mongodb.c) | [`res_config_mongodb.conf`](test_bench/configs/res_config_mongodb.conf)<br>[`sorcery.conf`](test_bench/configs/sorcery.conf)<br>[`extconfig.conf`](test_bench/configs/extconfig.conf) | as realtime configuration engine
`cdr_mongodb.so`       |   | * |   | [`cdr_mongodb.c`](src/cdr_mongodb.c)|[`cdr_mongodb.conf`](test_bench/configs/cdr_mongodb.conf)
`cel_mongodb.so`       |   |   | * | [`cel_mongodb.c`](src/cel_mongodb.c)|[`cel_mongodb.conf`](test_bench/configs/cel_mongodb.conf)


## How to get the plugins

The plugins are provided as source code patches to Asterisk.
See [src](src) and [patches](patches) in detail.

## Test
See [Test](test/docker).

## ~~Test bench~~
**(Deprecated, use [Test](test/docker) instead of it)**

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

### Config files as example
- *CHANGE@v0.3*: The three config files `res_config_mongodb.conf`, `cdr_mongodb.conf` and `cel_mongodb.conf` have compiled into one `ast_mongo.conf`.
- [`ast_mongo.conf`](test_bench/configs/ast_mongo.conf) for ast_mongo plugins;

        ;==========================================
        ;
        ; for common configuration
        ;
        [common]
        ;------------------------------------------
        ; MongoDB C Driver - Logging configuration
        ; see http://mongoc.org/libmongoc/current/logging.html in detail
        ;
        ; -1 = disable logging of mongodb-c-driver
        ; 0 = MONGOC_LOG_LEVEL_ERROR,
        ; 1,2,...,
        ; 6 = MONGOC_LOG_LEVEL_TRACE
        ; default is -1
        ;mongoc_log_level=-1
        ;------------------------------------------
        ; MongoDB C Driver - APM configuration
        ; see http://mongoc.org/libmongoc/current/application-performance-monitoring.html in detail
        ;
        ; 0  = disable monitoring
        ; 0 != enable monitoring
        ; default is 0
        ;apm_command_monitoring=0
        ;apm_sdam_monitoring=0
        ;==========================================
        ;
        ; for realtime configuration engine
        ;
        [config]
        uri=mongodb://mongodb.local/asterisk    ; location of database
        ;------------------------------------------
        ; 0 != enable APM
        ; default is disabled (0)
        ;apm=0
        ;==========================================
        ;
        ; for CDR plugin
        ;
        [cdr]
        uri=mongodb://mongodb.local/cdr ; location of database
        database=cdr                    ; name of database
        collection=cdr                  ; name of collection to record cdr data
        ;------------------------------------------
        ; 0 != enable APM
        ; default is disabled (0)
        ;apm=0
        ;==========================================
        ;
        ; for CEL plugin
        ;
        [cel]
        uri=mongodb://mongodb.local/cel ; location of database
        database=cel                    ; name of database
        collection=cel                  ; name of collection to record cel data
        ;------------------------------------------
        ; 0 != enable APM
        ; default is disabled (0)
        ;apm=0

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

        ; For queue logging (preliminarly)
        ;queue_log => mongodb,queues,queue_log

- See Asterisk's official document [Setting up PJSIP Realtime][5] as well.

## Supporting library
- [`ast_mongo_ts`](https://github.com/minoruta/ast_mongo_ts) which is nodejs library
provides functionalities to handle asterisk's object through MongoDB.

## License and Copyright

- The related code to Asterisk:
    - GNU GENERAL PUBLIC LICENSE Version 2
- Any other resources and files:
    - The MIT License (MIT)
- Copyright: (C) 2016-2018, KINOSHITA minoru, [viktike][9] for cel_mongodb


[1]: http://asterisk.org/        "Asterisk"
[2]: https://mongodb.org/        "MongoDB"
[3]: https://github.com/mongodb/mongo-c-driver  "mongo_c_driver"
[4]: http://www.pjsip.org       "PJSIP"
[5]: https://wiki.asterisk.org/wiki/display/AST/Setting+up+PJSIP+Realtime
[9]: https://github.com/viktike
