import * as DEBUG from 'debug';
import * as path from 'path';
import { AstMongo, AstMongoOptions } from 'ast_mongo_ts';
import { AstUtils, AstUtilsConfg } from 'ast_utils';

const debug = DEBUG('AST_MONGO:tester');

const ENV = process.env;
const HostAddress = ENV.ASTERISK_ADDRESS || '127.0.0.1';
const AmiUser = 'asterisk';
const AmiPassword = 'asterisk';

const astMongoOptions: AstMongoOptions = {
    urls: {
        config: ENV.MONGO_CONFIG || ENV.npm_package_config_config || 'mongodb://127.0.0.1:27017/config_test',
    },
};

const astUtilsConfig: AstUtilsConfg = {
    host: HostAddress,
    ari: {
        protocol: 'http',
        port: 8088,
        username: AmiUser,
        password: AmiPassword
    },
    ami: {
        port: 5038,
        username: AmiUser,
        password: AmiPassword
    }
};

let ast_mongo: AstMongo;
let ast_utils: AstUtils;
let my_realtime_trunk_id: String;

function delay(sec: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, sec * 1000));
}

/**
 * Delete a property from the specified object.
 */
function ignore(obj: object): object {
    const result = obj.toObject();
    result._id = "000000000000000000000000";
    return result;
}

/**
 * Delete a property from the specified array.
 */
function ignores(objs: object[]): object {
    let results = [];
    objs.forEach(obj => results.push(ignore(obj)));
    return results;
}

function ignoreIdentify(result: object) {
    const filter = /Identify\:  [0-9a-f]{24}\//gm;
    result.Output = result.Output.replace(filter, "Identify:  000000000000000000000000/");
    return result;
}

beforeAll( async () => {
    global.Promise = Promise;

    debug('connecting AstUtils...');
    ast_utils = new AstUtils(astUtilsConfig);
    await ast_utils.connect();

    debug('connecting AstMongo...');
    ast_mongo = new AstMongo(astMongoOptions);
    await ast_mongo.connect();

    await ast_mongo.Identify.remove({});
});

afterAll(() => {
    ast_utils.disconnect();
});

describe('ast_mongo', () => {

    test ('check if there is no identifies', async () => {
        const identifies = await ast_utils.exec('pjsip show identifies');
        expect(identifies).toMatchSnapshot();
    });

    test ('write an identify', async () => {
        const result = await ast_mongo.Identify.create({
            endpoint: 'my_realtime_trunk',
            match: '203.0.113.11'
        });
        my_realtime_trunk_id = result._id;
        expect(ignore(result)).toMatchSnapshot();
    });

    test ('write two identifies', async () => {
        const results = await ast_mongo.Identify.create([{
            endpoint: 'my_realtime_trunk',
            match: '203.0.113.12'
        }, {
            endpoint: 'my_realtime_trunk',
            match: '203.0.113.13'
        }]);
        expect(ignores(results)).toMatchSnapshot();
    });

    test ('check the identifies', async () => {
        const identifies = await ast_utils.exec('pjsip show identifies');
        debug('check the identifies', identifies);
        expect(ignoreIdentify(identifies)).toMatchSnapshot();
    });

    test ('check the identify', async () => {
        const identify = await ast_utils.exec(`pjsip show identify ${my_realtime_trunk_id}`);
        debug('check the identify', identify);
        expect(ignoreIdentify(identify)).toMatchSnapshot();
    });

    test ('remove the identify', async () => {
        await ast_mongo.Identify.remove({ endpoint: 'my_realtime_trunk' });

        const identifies = await ast_utils.exec('pjsip show identifies');
        expect(identifies).toMatchSnapshot();
    });
});
