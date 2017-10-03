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
    var test_ep = { id: "6001", password: "PW6001", tech: "PJSIP" };

    var test_records = {
        ps_aors: [
            { _id: test_ep.id, max_contacts: '10' },
        ],
        ps_auths: [
            { _id: test_ep.id, password: test_ep.password, username: test_ep.id, auth_type: "userpass" },
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
        testlib.ari.getEndpointStatus(test_ep.id, test_ep.tech, function(err, status) {
            if (err) {
                var message = JSON.parse(err.message);
                expect(message.message).to.be.equal('Endpoint not found');
                done();
            }
            else {
                expect(status).to.be.oneOf(["offline", "unknown"]);
                done();                
            }
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
            uas.stop();
            expect(cause.status_code).to.be.equal(401);
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
            uas.stop();
            expect(cause.status_code).to.be.equal(401);
            done();
        });

        uas.start();
    })

    it("with right account should be online", function(done) 
    {
        var uas = testlib.sip.getUA(test_ep.id, test_ep.password);

        uas.on('registered', function() {
            testlib.ari.getEndpointStatus(test_ep.id, test_ep.tech, function(err, status) {
                uas.stop();
                expect(err).to.be.undefined;
                expect(status).to.be.equal("online");
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
                uas.stop();
                if (cause.status_code != 401 && cause.status_code != 500)
                    assert.isNotOk("status was not 401 nor 500", cause.status_code);
                // expect(cause.status_code).to.be.equal(401);
                done();
            });

            uas.start();
        });
    })

    it("should be offline at last", function(done) 
    {
        testlib.ari.getEndpointStatus(test_ep.id, test_ep.tech, function(err, status) {
            expect(err).to.be.undefined;
            expect(status).to.be.equal("offline");
            done();
        });
    })

    afterEach(function() {
        setTimeout(function() {
            done();
        }, 3000);
    })
});
