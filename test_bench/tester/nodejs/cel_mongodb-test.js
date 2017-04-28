"use strict";
/**
 * Function Test for cel_mongodb
 *
 * Copyright: (C) 2016 KINOSHITA minoru
 * License: The MIT License (MIT)
 *
 */
var expect = require('chai').expect;
var assert = require('chai').assert;
var async = require('async');
var testlib = require('./testlib');
var config = require('./config').mongodb.cel;
config.db = require('mongodb-uri').parse(config.uri).database;
var _ = require('lodash');
//
//  Expected test result
//
var expected = require('./cel');
//
//  Loging test for cel_mongodb
//
describe('CEL by cel_mongodb', function() 
{
    var uas;
    var uac;

    var test_ep = [
        {id: "6001", password: "PW6001", tech: "PJSIP", },
        {id: "6002", password: "PW6002", tech: "PJSIP", },
    ];

    var test_records = {
        ps_aors: [
            { _id: test_ep[0].id, max_contacts: '10', },
            { _id: test_ep[1].id, max_contacts: '10', },
        ],
        ps_auths: [
            { _id: test_ep[0].id, password: test_ep[0].password, username: test_ep[0].id, auth_type: "userpass", },
            { _id: test_ep[1].id, password: test_ep[1].password, username: test_ep[1].id, auth_type: "userpass", },
        ],
        ps_endpoints: [
            { _id:test_ep[0].id, aors:test_ep[0].id, auth:test_ep[0].id, transport:"transport-ws", context:"context1", disallow:"all", allow:"ulaw", direct_media:"no", },
            { _id:test_ep[1].id, aors:test_ep[1].id, auth:test_ep[1].id, transport:"transport-ws", context:"context1", disallow:"all", allow:"ulaw", direct_media:"no", },
        ],
    };

    before(function(done) {
        async.waterfall([
            function(callback) {
                testlib.mongo.loadCollections("config", test_records, callback);
            },
            function(callback) {
                testlib.mongo.cleanCollection("cel", config.db, callback);
            },
            function(callback) {
                uas = testlib.sip.getUA(test_ep[0].id, test_ep[0].password);
                uas.on('registered', function() {
                    callback();
                });
                uas.on('registrationFailed', function(cause) {
                    uas.stop();
                    assert.isNotOk("registration failed", cause.status_code);
                });
                uas.on('invite', function(session) {
                    session.reject();
                });
                uas.start();
            },
            function(callback) {
                uac = testlib.sip.getUA(test_ep[1].id, test_ep[1].password);
                uac.on('registered', function() {
                    callback();
                });
                uac.on('registrationFailed', function(cause) {
                    uas.stop();
                    assert.isNotOk("registration failed", cause.status_code);
                });
                uac.start();
            },
        ], done);
    })

    it("should log event transactions", function(done) 
    {
        var session = testlib.sip.makeCall(uac, test_ep[0].id);
        session.on('rejected', function(response, cause) {

            testlib.mongo.getCels(function(cels) 
            {
                setTimeout(function() {
                // in case of testing with replica set, 
                // tester should wait for a while
                // until a result has been updated in its replica set.
                    expect(cels.length).to.be.equal(expected.cels.length);
                    var i = 0;
                    _.forEach(expected.cels, function(expected_cel) {
                        var cel = cels[i];
                        _.forEach(expected_cel, function(value, key) {
                            var regexp = new RegExp(value);    
                            expect(cel[key]).to.match(regexp);
                        });
                        i += 1;
                    });
                    session.terminate();
                    done();
                }, 1000);
            });
        });
    })

    after(function(done) {
        uas.stop();
        uac.stop();
        // to close websocket properly.
        setTimeout(function() {
            done();
        }, 1000);
    })
});
