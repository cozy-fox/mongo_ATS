## Experimental test on docker

### Test
- open a terminal as #1
    - `docker-compose build`
    - `docker-compose up`
- open a terminal as #2
    - `docker exec -it tester ash`
    - `cd ast_mongo`
    - `npm install`
    - `npm test`

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
