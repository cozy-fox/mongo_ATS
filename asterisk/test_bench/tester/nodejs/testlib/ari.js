"use strict";
/**
 * Test Lib for ARI
 *
 * Copyright: (C) 2016 KINOSHITA minoru
 * License: The MIT License (MIT)
 *
 */
var ARI = require('ari-client');
var assert = require('chai').assert;
var config = require('../config').ari;

/**
 *  get endpoint status via ari
 *
 *  @param {string} id - id of the endpoint
 *  @param {string } tech - tech of the endpoint
 *  @param {Function} callback - callback
 */
exports.getEndpointStatus = function(id, tech, callback) {
    ARI
    .connect(config.uri, config.account.user, config.account.password)
    .then(function(ari) {
        ari.endpoints.get({
            resource: id,
            tech: tech, 
        },    
        function(err, endpoint) {
            if (err)
                return callback(err);
            callback(undefined, endpoint.state);
        });
    })
    .catch(function (err) {
        assert.isNull(err, `unexpected error, ${config.url}, ${err}`);
    });
}
