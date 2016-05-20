# MongoDB Plugins for Asterik

[This VM][6] provides the following plugins for [Asterisk][1];

1. [MongoDB][2] configuration engine,
2. MongoDB CDR backend,

and the development environment as well.

## Launch a VM to build and run Asteisk with the related modules

    desktop$ cd asterisk                ; select asterisk VM
    desktop$ vagrant up                 ; launch it and generate environment to build and run Asterisk

## Custom plugins

| module name           |for reatime|for CDR|related config file      |  Comment |
|-----------------------|-----------|-------|-------------------------|----------|
|`res_mongodb.so`       | require   |require|                         |as common library|
|`res_config_mongodb.so`| require   |       |`res_config_mongodb.conf`<br>`sorcery.conf`<br>`extconfig.conf`|as realtime configuration engine|
|`cdr_mongodb.so`       |           |require|`cdr_mongodb.conf`       |as cdr backend|

## Config files

- See Asterisk's official document [Setting up PJSIP Realtime][5] as well.

- [`res_config_mongodb.conf`](configs/res_config_mongodb.conf) for realtime configuration engine

        [mongodb]
        uri=mongodb://mongodb.local     ; location of database

- [`sorcery.conf`](configs/sorcery.conf) specifies map from asterisk's resources to database's collections.

        [res_pjsip]
        endpoint=realtime,ps_endpoints  ; map to ps_endpoints record
        auth=realtime,ps_auths          ; map to ps_auths record
        aor=realtime,ps_aors            ; map to ps_aors record

- [`extconfig.conf`](configs/extconfig.conf) specifies database for database's collections mapped above.

        [settings]
        extensions.conf => mongodb,asterisk,ast_config  ; map to ast_config record of asterisk database
        ps_endpoints => mongodb,asterisk                ; ps_endpoints record is in asterisk database
        ps_auths => mongodb,asterisk                    ; ps_auths record is in asterisk database
        ps_aors => mongodb,asterisk                     ; ps_aors record is in asterisk database

- [`cdr_mongodb.conf`](configs/cdr_mongodb.conf) specifies the location, name and collection of database for cdr backend.

        [mongodb]
        uri=mongodb://mongodb.local     ; location of database
        database=cdr                    ; name of database
        collection=cdr                  ; name of collection to record cdr data

## Vagrantfile

The following properties can be configurable with a common file [`config.json`](../config.json);

| Property            |Definition           | Defined |
|---------------------|---------------------|---------|
|[`mongodb`][2]       |version of the module| '3.2'   |
|[`mongo_c_driver`][3]|version of the module| '1.3.5' |
|[`pjsip`][4]         |version of the module| '2.4.5' |
|[`asterisk`][1]      |version of Asterisk  | '13.9.1'|

## Debug

- add `debug` to [`logger.conf`](configs/logger.conf).

        [logfiles]
        console => notice,warning,error,debug
        messages => notice,warning,error,debug

## License and Copyright

- License: GNU GENERAL PUBLIC LICENSE Version 2
- Copyright: (C) 2016, KINOSHITA minoru

[1]: http://asterisk.org/        "Asterisk"
[2]: https://mongodb.org/        "MongoDB"
[3]: https://github.com/mongodb/mongo-c-driver  "mongo_c_driver"
[4]: http://www.pjsip.org       "PJSIP"
[5]: https://wiki.asterisk.org/wiki/display/AST/Setting+up+PJSIP+Realtime
[6]: https://github.com/minoruta/ast_mongo/tree/master/asterisk
