import * as DEBUG from 'debug';
import * as path from 'path';
import {
    AstMongo, AstMongoOptions, StaticProperties,
    StaticModelHelper, StaticModelHelperCB,
    EndpointSet, EndpointSetHelper, EndpointSetHelperCB
} from 'ast_mongo_ts';
import { AstUtils, AstUtilsConfg } from 'ast_utils';
import { Pjsua,
PjsuaConfigs,
PlayerConfig,
AccountConfig,
AccountExt,
CallExt,
CallInfo,
} from 'pjsua';

const debug = DEBUG('AST_MONGO:tester');

const ENV = process.env;
const Domain = 'asterisk';
const MyID = '6001';
const BadID = 'nouser';
const CalleeLongID = '300';
const CalleeShortID = '301';
const HostAddress = ENV.ASTERISK_ADDRESS || '127.0.0.1';
const MyURI = `sip:${MyID}@${Domain}`;
const BadURI = `sip:${BadID}@${Domain}`;
const CalleeLongURI = `sip:${CalleeLongID}@${HostAddress}`;
const CalleeShortURI = `sip:${CalleeShortID}@${HostAddress}`;
const RegistrarURI = `sip:${HostAddress}:5060`;
const AmiUser = 'asterisk';
const AmiPassword = 'asterisk';
const ShortSound = 'demo-thanks';
const LongSound = 'demo-instruct';
const AstMakeAShortCall = `channel originate PJSIP/${MyID} application playback ${ShortSound}`;
const AstMakeALongCall = `channel originate PJSIP/${MyID} application playback ${LongSound}`;

const LOG_LEVEL = ENV.LOG_LEVEL || ENV.npm_package_config_pjsua_loglevel || 3;
const astMongoOptions: AstMongoOptions = {
    urls: {
        config: ENV.MONGO_CONFIG || ENV.npm_package_config_config || 'mongodb://127.0.0.1:27017/config_test',
        cdr: ENV.MONGO_CDR || ENV.npm_package_config_cdr || 'mongodb://127.0.0.1:27017/cdr_test',
        cel: ENV.MONGO_CEL || ENV.npm_package_config_cel || 'mongodb://127.0.0.1:27017/cel_test'
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

const unique_id = '000000000000000000000001';

let ua: Pjsua;
let ast_mongo: AstMongo;
let ast_utils: AstUtils;
let smh: StaticModelHelper;
let esh: EndpointSetHelper;

class Timeout {
    private static readonly DEFAULT_TIMEOUT = 20 * 1000; // msec
    private readonly _saved: number;
    constructor(timeout = Timeout.DEFAULT_TIMEOUT) {
        this._saved = jasmine.DEFAULT_TIMEOUT_INTERVAL;
        jasmine.DEFAULT_TIMEOUT_INTERVAL = timeout;
    }
    end(): void {
        jasmine.DEFAULT_TIMEOUT_INTERVAL = this._saved;
    }
}

function delay(sec: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, sec * 1000));
}

beforeAll( async () => {

    global.Promise = Promise;
    jest.setTimeout(60 * 1000);

    debug('connecting AstMongo...');
    ast_mongo = new AstMongo(astMongoOptions);
    await ast_mongo.connect();

    debug('connecting AstUtils...');
    ast_utils = new AstUtils(astUtilsConfig);
    await ast_utils.connect();

    smh = new StaticModelHelper(ast_mongo.Static, unique_id);
    esh = new EndpointSetHelper(ast_mongo);

    await ast_mongo.Aor.remove({});
    await ast_mongo.Auth.remove({});
    await ast_mongo.Endpoint.remove({});
    await ast_mongo.Cdr.remove({});
    await ast_mongo.Cel.remove({});

    debug('preparing pjsip.conf...');
    const pjsip = await smh.create(
        'pjsip.conf', 'transport-udp', [{
            type: 'transport',
            protocol: 'udp',
            bind: '0.0.0.0'
        }]);
    await ast_mongo.Static.create(pjsip);
    await ast_utils.reload(10 * 1000);

    debug('preparing extensions.conf...');
    const extensions = await smh.create(
        'extensions.conf', 'default', [
            // { exten: '300,1,Playback(demo-instruct)'},
            // { exten: '300,n,Hangup()'},
            // { exten: '301,1,Playback(demo-instruct)'},
            // { exten: '301,n,Hangup()'},
            { exten: '_6.,1,NoOp()'},
            { exten: '_6.,n,Dial(PJSIP/${EXTEN})'},
            { exten: '_6.,n,Hangup()'},
        ]);
    await ast_mongo.Static.create(extensions);
    await ast_utils.reloadDialPlan();

    debug('create an endpoint');
    await esh.create(MyID, {
        endpoint: {
            context: 'default',
            allow: 'ulaw',
            transport: 'transport-udp',
            rewrite_contact: true
        },
        aor: {
            max_contacts: 1,
            remove_existing: true,
            contact: `sip:${MyID}@tester:5060`
        },
        auth: {
            auth_type: 'userpass',
            password: MyID,
            username: MyID
        }
    });
});

afterAll(() => {
    ast_utils.disconnect();
});

describe('ast_mongo', () => {

    test ('check pjsip resources of the remote asterisk', async () => {
        // 1
        const dialplan = await ast_utils.exec('dialplan show');
        expect(dialplan).toMatchSnapshot();
        // 2
        const transports = await ast_utils.exec('pjsip show transports');
        expect(transports).toMatchSnapshot();
        // 3
        const transport = await ast_utils.exec('pjsip show transport transport-udp');
        expect(transport).toMatchSnapshot();
        // 4
        const endpoints = await ast_utils.exec('pjsip show endpoints');
        // modify to compare with the snap shot
        endpoints.Output = endpoints.Output.replace(/Not in use /g, "Unavailable");
        endpoints.Output = endpoints.Output
            .replace(/Contact:  6001\/sip:6001@172(.*)Unknown(.*)\n/gm, "")
            .replace(/Contact:  6001\/sip:6001@tester:5060                  9d7987fe43 Unknown         nan$/gm,
                      "Contact:  6001/sip:6001@tester:5060                  9d7987fe43 Created       0.000");
        expect(endpoints).toMatchSnapshot();
        // 5
        const endpoint = await ast_utils.exec(`pjsip show endpoint ${MyID}`);
        // modify to compare with the snap shot
        const result = endpoint.Output
            .replace(/Contact:  6001\/sip:6001@172(.*)\n/gm, "")
            .replace(/Contact:  6001\/sip:6001@tester:5060                  9d7987fe43 Unknown         nan$/gm,
                      "Contact:  6001/sip:6001@tester:5060                  9d7987fe43 Created       0.000");
        endpoint.Output = result;
        expect(endpoint).toMatchSnapshot();
        // 6
        const aors = await ast_utils.exec('pjsip show aors');
        // modify to compare with the snap shot
        aors.Output = aors.Output.replace(/^Contact:  6001\/sip:6001@172(.*)\n/gm, "");
        expect(aors).toMatchSnapshot();
        // 7
        const aor = await ast_utils.exec(`pjsip show aor ${MyID}`);
        // modify to compare with the snap shot
        aor.Output = aor.Output
            .replace(/^=+\n/gm, "")
            .replace(/Contact:  6001\/sip:6001@172(.*)\n/gm, "")
            .replace(/contact              : sip:6001@172(.*)\n/gm, "");
        expect(aor).toMatchSnapshot();
        // 8
        const auths = await ast_utils.exec('pjsip show auths');
        expect(auths).toMatchSnapshot();
        // 9
        const auth = await ast_utils.exec(`pjsip show auth ${MyID}`);
        expect(auth).toMatchSnapshot();
    });

    test ('create a pjsua', done => {
        debug('creating a ua', __dirname);
        const config: PjsuaConfigs = {
            endpoint: {
                logConfig: {
                    level: LOG_LEVEL,
                    consoleLevel: LOG_LEVEL
                }
            },
            transport: {
                port: 5060,
            },
            player: {
                player: {
                    filename: path.join(__dirname, '../waves', 'sound.wav')
                },
                recorder: {
                    filename: path.join(__dirname, '../waves', 'call.wav')
                }
            }
        };
        ua = new Pjsua(config);
        done();
    });

    test ('make an account', async () => {
        debug('making an account');
        const config: AccountConfig = {
            idUri: MyURI,
            regConfig: {
                registrarUri: RegistrarURI,
            },
            sipConfig: {
                authCreds: [{
                    scheme: 'digest',
                    realm: Domain,
                    username: MyID,
                    dataType: 0,
                    data: MyID
                }],
            }
        };
        await ua.makeAccount(config);
    });

    test ('wait for a call, accept it, then disconnect from far', async (done) => {
        ua.account.once('call', async (info: CallInfo, call: CallExt) => {
            debug('account.onCall & accept it');
            await ua.account.answer(call);
            ua.account.call.once('disconnected', async () => {
                debug('ua.account.call.disconnected');
                await delay(1); // to check cdr and cel
                done();
            });
        });
        debug('make an inbound call');
        const result = await ast_utils.exec(AstMakeAShortCall);
        expect(result).toMatchSnapshot();
    });

    test ('check cdr', async () => {
        const results = await ast_mongo.Cdr
            .find()
            .sort('start')
            .select('-_id -start -answer -end -linkedid -uniqueid');
        // modify to compare with the snap shot
        const objs = JSON.parse(JSON.stringify(results));
        objs.forEach(result => {
            if (result.channel)
                result.channel = result.channel.split('-')[0];
            if (result.sequence && typeof result.sequence === 'number')
                result.sequence = 0;
        });
        expect(objs).toMatchSnapshot();
    });

    test ('check cel', async () => {
        const results = await ast_mongo.Cel
            .find()
            .sort('eventtime')
            .select('-_id -eventtime -linkedid -uniqueid');
        // modify to compare with the snap shot
        const objs = JSON.parse(JSON.stringify(results));
        objs.forEach(result => {
            if (result.extra)
                result.extra = JSON.parse(result.extra);
            if (result.channame)
                result.channame = result.channame.split('-')[0];
        });
        expect(objs).toMatchSnapshot();
    });
});
