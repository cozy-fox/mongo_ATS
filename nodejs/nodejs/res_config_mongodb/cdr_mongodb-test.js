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
var testlib = require('./testlib');
var config = require('./config').mongodb.cdr;

//
//  Loging test for cdr_mongodb
//
describe('CDR by cdr_mongodb', function() 
{
    var uas;
    var uac;

    var test_ep = [
        {id: "6001", password: "6001", tech: "PJSIP", },
        {id: "6002", password: "6002", tech: "PJSIP", },
    ];

    var test_records = {
        ps_aors: [
            { _id: test_ep[0].id, max_contacts: '10', },
            { _id: test_ep[1].id, max_contacts: '10', },
        ],
        ps_auths: [
            { _id: test_ep[0].id, password: test_ep[0].id, username: test_ep[0].id, auth_type: "userpass", },
            { _id: test_ep[1].id, password: test_ep[1].id, username: test_ep[1].id, auth_type: "userpass", },
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

            testlib.mongo.getCdrs(function(cdrs) 
            {
                expect(cdrs.length).to.be.equal(1);
                var cdr = cdrs[0];

                expect(cdr.clid).to.be.equal(`\"\" <${test_ep[1].id}>`);
                expect(cdr.src).to.be.equal(test_ep[1].id);
                expect(cdr.dst).to.be.equal(test_ep[0].id);
                expect(cdr.dcontext).to.be.equal("context1");
                expect(cdr.channel.split("-")[0]).to.be.equal(`${test_ep[1].tech}/${test_ep[1].id}`); 
                expect(cdr.dstchannel.split("-")[0]).to.be.equal(`${test_ep[0].tech}/${test_ep[0].id}`);
                expect(cdr.lastapp).to.be.equal("Dial");
                expect(cdr.lastdata).to.be.equal(`${test_ep[0].tech}/${test_ep[0].id}`);
                expect(cdr.disposition).to.be.equal("NO ANSWER");
                expect(cdr.amaflags).to.exist;
                expect(cdr.accountcode).to.exist;
                expect(cdr.uniqueid).to.exist;
                expect(cdr.userfield).to.exist;
                expect(cdr.peeraccount).to.exist;
                expect(cdr.linkedid).to.exist;
                expect(cdr.duration).to.be.equal(0);
                expect(cdr.billsec).to.be.equal(0);
                expect(cdr.sequence).to.exist;
                expect(cdr.start).to.exist;
                expect(cdr.answer).to.exist;
                expect(cdr.end).to.exist;

                session.terminate();
                done();
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
