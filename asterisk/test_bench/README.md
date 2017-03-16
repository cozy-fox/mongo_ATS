
## Testing Configuration

A testing environment of this test bench is consisted of Docker technorogy and the configuration is as follows;

```
+---------+
| Your    +--(Shared source code under ../src) 
| Desktop +--(Shared resources under ./configs for example)
+----+----+
     |   docker's virtual network (ast_mongo)
-----+--------------+-------------+----------
     |              |             |
+----+----+    +----+----+   +----+----+
| Target  |    | MongoDB |+  | Tester  |
| Asterisk|    |         ||+ |         |
+---------+    +---------+|| +---------+
asterisk.local  +---------+| tester.local
                 +---------+
     ast_mongo#.local as member of ast_mongo_set
```

As you can see above, the test bench deploys three mongodb instances as MongoDB replica set 
as well as two other instances.
It's however not essential.

### Docker's resources

#### Docker Network

Network name | Description
-------------|---------
`ast_mongo`    | a common network for this test bench.


#### Docker Containers

Container Name   | Base Image | The related script | comment
-----------------|------------|--------------------|--------
`asterisk.local`        | [ubuntu:xenial](https://hub.docker.com/_/ubuntu/) | [./asterisk/asterisk](asterisk/asterisk)
`ast_mongo[1..3].local` | [mongo](https://hub.docker.com/_/mongo/)          | [./mongodb/mongors](mongodb/mongors) | constructs a replica set '`ast_mongo_set`'.
`tester.local`          | [node:boron](https://hub.docker.com/_/node/)      | [./tester/tester](tester/tester)

## Test Command

### `./test`
allows you to run a whole test of ast_mongo with this test bench.

1. prepare following resources to test,
    1. an `ast_mongo` as network,
    1. a replica set `ast_mongo_set` which consists of `ast_mongo1.local`,`ast_mongo2.local` and `ast_mongo3.local`,
    1. an `asterisk.local` as a target container for asterisk,
    1. a `tester.local` as tester container.
1. build a target asterisk on `asterisk.local`, and generate a new patch under [`../patches`](../patches) from source.
1. test a target `asterisk.local` by `tester.local` with `ast_mongo_set`.


### `./test halt`
allows you to stop activities of outstanding containers.
It's eauivalent to;
- `docker container stop asterisk.local tester.local ast_mongo1.local ast_mongo2.local ast_mongo3.local`

### `./test clean`
allows you to clean up all of outstanding resouces.
It's eauivalent to;
- `docker container rm -f asterisk.local tester.local ast_mongo1.local ast_mongo2.local ast_mongo3.local`
- `docker network rm ast_mongo`

### `./test asterisk`
allows you to connect a command shell of the asterisk container.
It's eauivalent to;
- `docker exec -it asterisk.local bash`

### `./test tester`
allows you to connect a command shell of the tester container.
It's eauivalent to;
- `docker exec -it tester.local bash`

### `./test ast_mongo[1..3]`
allows you to connect a mongo shell of the specified container.
It's eauivalent to;
- `docker exec -it ast_mongo[1..3].local mongo`

### How to test

- run `./test`
  ```
$ git clone https://github.com/minoruta/ast_mongo.git
$ cd ast_mongo/asterisk/test_bench
$ ./test

...
... prepare containers, build asteriks with ast_mongo's patches.
...

container tester.local started.


  A PJSIP UAS managed by res_config_mongodb
    ✓ should be offline at first (302ms)
    ✓ with unknown user should get 401 (435ms)
    ✓ with invalid password should get 401 (54ms)
    ✓ with right account should be online (149ms)
    ✓ with invalidated account should get 401 (131ms)
    ✓ should be offline at last (65ms)


  6 passing (3s)



  CDR by cdr_mongodb
    ✓ should log a call transaction (344ms)


  1 passing (2s)



  CEL by cel_mongodb
    ✓ should log event transactions (109ms)


  1 passing (2s)

=================================
end of test
=================================
```


- run `./test clean` to clean up outstanding resources.
  ```
$ ./test clean
=================================
clean the outstanding resources: asterisk, tester, ast_mongo1, ast_mongo2, ast_mongo3 and ast_mongo
=================================
CONTAINER ID        IMAGE               COMMAND                  CREATED             STATUS                   PORTS               NAMES
............        .....               ......                   .....               ......                   .....               .....
NETWORK ID          NAME                DRIVER              SCOPE
............        .....               ......              .....
```

## Versioning of Asterisk and its related libraries

You can specify versions of some essential libraries to build to a [`config.json`](../../config.json) file;

Property             |Definition           | Comments
---------------------|---------------------|----------
`mongo_c_driver` |version of the [mongo c driver][3] |
`pjsip`          |version of the [pjsip][4] | note: use the pjsip bundled with asterisk if you specify '2.5' or later.
`asterisk`       |version of [Asterisk][1]  |
`mongodb`        |version of the module| deprecated

## Requirements

- [Docker](https://www.docker.com)
  - e.g. [Docker Community Edition for Mac, version 17.03.0-ce-mac2](https://store.docker.com/editions/community/docker-ce-desktop-mac)

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
[9]: https://github.com/viktike
