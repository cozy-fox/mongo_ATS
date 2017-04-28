"use strict";
/**
 * Function Test for cdr_mongodb
 *
 * Copyright: (C) 2016 KINOSHITA minoru
 * License: The MIT License (MIT)
 *
 */
var expect = require('chai').expect;
var assert = require('chai').assert;
var async = require('async');
var _ = require('lodash');
var testlib = require('./testlib');
var config = require('./config').mongodb.cdr;
config.db = require('mongodb-uri').parse(config.uri).database;

//
//  Expected test result
//
var expected = require('./cdr');

//
//  Loging test for cdr_mongodb
//
describe('CDR by cdr_mongodb', function() 
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
                testlib.mongo.cleanCollection("cdr", config.db, callback);
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

    it("should log a call transaction", function(done) 
    {
        var session = testlib.sip.makeCall(uac, test_ep[0].id);
        session.on('rejected', function(response, cause) {
            setTimeout(function() {
            // in case of testing with replica set, 
            // tester should wait for a while
            // until a result has been updated in its replica set.
                testlib.mongo.getCdrs(function(cdrs) 
                {
                    expect(cdrs.length).to.be.equal(1);
                    var cdr = cdrs[0];
                    _.forEach(expected, function(value, key) {
                        var regexp = new RegExp(value);    
                        expect(cdr[key]).to.match(regexp);
                    });
                    session.terminate();
                    done();
                });
            }, 1000);
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
