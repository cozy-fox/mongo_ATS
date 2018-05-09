## Experimental test on docker

### Test
- Specify the versions to test in `docker-compose.yml`
```yml
  services:
    asterisk:
      build:
        args:
          VERSION_ASTERISK: 15.4.0
          VERSION_MONGOC: 1.9.5
          VERSION_LIBSRTP: 2.1.0        
```
- open a terminal as #1
    - `docker-compose build`
    - `docker-compose up [-d]`
- then you've got a patch for the specified asterisk version as well;
    - `../../patches/ast_mongo-$VERSION_ASTERISK.patch`
- open a terminal as #2
    - `docker exec -it tester ash`
    - `cd ast_mongo`
    - `npm install`
    - `npm test`
- clean up outstanding docker resources;
    - `docker-compose down`

### Containers for testing
#### Asterisk container
| Name of image | Description      |
|---------------|------------------|
|  asterisk     | Asterisk container |
| ubuntu:latest | Ubuntu |

#### Tester container
| Name of image | Description      |
|---------------|------------------|
|  tester       | Tester container |
| [minoruta/pjsip-node-alpine](https://github.com/minoruta/pjsip-node-alpine) | pjsip libraries |
| [minoruta/node-alpine](https://github.com/minoruta/node-alpine) | nodejs |
| [alpine](https://alpinelinux.org) | alpine linux |

#### MongoDB container
| Name of image | Description      |
|---------------|------------------|
|  ast_mongo    | MongoDB container |
| mongo:latest  | MongoDB |
