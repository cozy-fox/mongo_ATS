"use strict";
/**
 * Test Lib for SIP
 *
 * Copyright: (C) 2016 KINOSHITA minoru
 * License: The MIT License (MIT)
 *
 */
var SIP = require('sip.js');
var assert = require('chai').assert;
var async = require('async');
var _ = require('lodash');
var config = require('../config').sip;
var asterisk = require('../config').asterisk;

/**
 *  get new UA of SIP
 *
 *  @param {string} id - id of the endpoint
 *  @param {string } password - password of the endpoint
 */
exports.getUA = function(id, password) {

    var configuration = _.cloneDeep(config);
    configuration.uri = id + "@" + asterisk.host;
    configuration.password = password;
    configuration.wsServers = [asterisk.uri];

    return new SIP.UA(configuration);
}

/**
 *  make a call
 *  Note:   this is not for making a real communition,
 *          just for making a record into CDR to test.
 *
 *  @param {SIP.UA} userAgent - ua as caller
 *  @param {string} dst - id of callee
 */
exports.makeCall = function(userAgent, dst) {

    //
    //  NOTE: provide pseudo media handler to make a session by force.
    //
    var close = function() {
    }
    userAgent.configuration.mediaHandlerFactory.isSupported = function() {
        return true;
    }
    SIP.WebRTC.isSupported = function() {
        return true;
    }
    SIP.WebRTC.RTCPeerConnection = function() {
        return {
            close: close
        }
    }
    SIP.WebRTC.RTCSessionDescription = function(rawDescription) {
        return {
            type: "answer",
            sdp: new String(rawDescription.sdp)
        };
    }
    var options = {
        inviteWithoutSdp: true,
        rendertype: "application/sdp",
        renderbody: [
                "v=0",
                "o=- 273268640 273268640 IN IP4 127.0.0.0",
                "s=-",
                "c=IN IP4 127.0.0.0",
                "t=0 0",
                "m=audio 10000 RTP/AVP 0",
                "a=rtpmap:0 PCMU/8000",
                ""
        ].join("\r\n"),
    }

    var target = "sip:" + dst + "@" + asterisk.host;
    var session = userAgent.invite(target, options);
    return session;
}
