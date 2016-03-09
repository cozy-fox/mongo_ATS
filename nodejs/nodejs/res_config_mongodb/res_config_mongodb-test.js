"use strict";
/**
 * Function Test for res_config_mongodb
 *
 * Copyright: (C) 2016 KINOSHITA minoru
 * License: The MIT License (MIT)
 *
 */
var expect = require('chai').expect;
var assert = require('chai').assert;
var async = require('async');
var testlib = require('./testlib');

//
//  UAS registration test
//
describe('A PJSIP UAS managed by res_config_mongodb', function() 
{
    var test_ep = { id: "6001", password: "6001", tech: "PJSIP" };

    var test_records = {
        ps_aors: [
            { _id: test_ep.id, max_contacts: '10' },
        ],
        ps_auths: [
            { _id: test_ep.id, password: test_ep.id, username: test_ep.id, auth_type: "userpass" },
        ],
        ps_endpoints: [
            { _id: test_ep.id, aors: test_ep.id, auth: test_ep.id, transport: "transport-ws", context: "context1", disallow: "all", allow: "ulaw", direct_media: "no" },
        ],
    };

    before(function(done) {
        testlib.mongo.loadCollections("config", test_records, done);
    })

    it("should be offline at first", function(done) 
    {
        testlib.ari.getEndpointStatus(test_ep.id, test_ep.tech, function(status) {
            expect(status).to.be.equal("offline");
            done();
        });
    })

    it("with unknown user should get 401", function(done) 
    {
        var uas = testlib.sip.getUA(test_ep.id + "UNKNOWN", test_ep.password);

        uas.on('registered', function() {
            uas.stop();
            assert.isNotOk("registered");
        });
        uas.on('registrationFailed', function(cause) {
            expect(cause.status_code).to.be.equal(401);
            uas.stop();
            done();
        });

        uas.start();
    })

    it("with invalid password should get 401", function(done) 
    {
        var uas = testlib.sip.getUA(test_ep.id, test_ep.password + "INVALID");

        uas.on('registered', function() {
            uas.stop();
            assert.isNotOk("registered");
        });
        uas.on('registrationFailed', function(cause) {
            expect(cause.status_code).to.be.equal(401);
            uas.stop();
            done();
        });

        uas.start();
    })

    it("with right account should be online", function(done) 
    {
        var uas = testlib.sip.getUA(test_ep.id, test_ep.password);

        uas.on('registered', function() {
            testlib.ari.getEndpointStatus(test_ep.id, test_ep.tech, function(status) {
                expect(status).to.be.equal("online");
                uas.stop();
                done();
            });
        });
        uas.on('registrationFailed', function(cause) {
            uas.stop();
            assert.isNotOk("registration failed", cause.status_code);
        });

        uas.start();
    })

    it("with invalidated account should get 401", function(done) 
    {
        testlib.mongo.cleanCollections("config", test_records, function() 
        {
            var uas = testlib.sip.getUA(test_ep.id, test_ep.password);

            uas.on('registered', function() {
                uas.stop();
                assert.isNotOk("registered");
            });
            uas.on('registrationFailed', function(cause) {
                expect(cause.status_code).to.be.equal(401);
                uas.stop();
                done();
            });

            uas.start();
        });
    })

    it("should be offline at last", function(done) 
    {
        testlib.ari.getEndpointStatus(test_ep.id, test_ep.tech, function(status) {
            expect(status).to.be.equal("offline");
            done();
        });
    })

    after(function(done) {
        // to close websocket properly.
        setTimeout(function() {
            done();
        }, 1000);
    })
});
