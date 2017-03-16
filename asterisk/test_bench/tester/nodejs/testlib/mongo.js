"use strict";
/**
 * Test Lib for MongoDB
 *
 * Copyright: (C) 2016 KINOSHITA minoru
 * License: The MIT License (MIT)
 *
 */
var MongoClient = require('mongodb').MongoClient;
var ObjectId = require('mongodb').ObjectID;
var assert = require('chai').assert;
var async = require('async');
var config = require('../config').mongodb;
var SERVER_ID = require('../config').SERVER_ID;

/**
 * get run-time serverId
 *
 *  @param {(string|number|ObjectId)} serverId - id of asterisk server
 *  @return {(string|ObjectId)} run-time server id
 */
var getServerId = function() {
    var serverId = SERVER_ID;
    if (!serverId)
        return null;
    if (!ObjectId.isValid(serverId))
        return serverId;
    return ObjectId(serverId);
}
exports.getServerId = getServerId;

/**
 *  load the specified collections into db
 *
 *  @param {string} dbName - name to database.
 *  @param {[object]} collections - record set of collections to be loaded.
 *  @param {Function} callback - callback
 */
exports.loadCollections = function(dbName, collections, callback) {

    var uri = config[dbName].uri;
    var serverId = getServerId();
    var conditions = serverId 
        ? {serverid: serverId} 
        : {};

    MongoClient.connect(uri, function(err, db) 
    {
        assert.isNull(err, `unexpected error, ${uri}, ${err}`);
        async.forEachOf(collections, function(records, collectionName, callback) 
        {
            db
            .collection(collectionName)
            .deleteMany(conditions, function(err, result) {
                assert.isNull(err, `unexpected error, ${collectionName}, ${conditions}, ${err}`);
                db
                .collection(collectionName)
                .insertMany(records, function(err, result) 
                {
                    assert.isNull(err, `unexpected error, ${collectionName}, ${err}`);
                    callback();
                });                    
            });
        }, function(err) {
            db.close();
            callback();
        });
    });
}

/**
 *  clean the specified collection of db
 *
 *  @param {string} dbName - name to database.
 *  @param {string} collectionName - name of collection to be cleaned.
 *  @param {Function} callback - callback
 */
exports.cleanCollection = function(dbName, collectionName, callback) {

    var uri = config[dbName].uri;
    var serverId = getServerId();
    var conditions = serverId 
        ? {serverid: serverId} 
        : {};

    MongoClient.connect(uri, function(err, db) 
    {
        assert.isNull(err, `unexpected error, ${uri}, ${err}`);
        db
        .collection(collectionName)
        .deleteMany(conditions, function(err, result) {
            assert.isNull(err, `unexpected error, ${collectionName}, ${err}`);
            db.close();
            callback();
        });
    });
}

/**
 *  clean the specified collection of db
 *
 *  @param {string} dbName - name to database.
 *  @param {[object]} collections - set of collections to be cleaned.
 *  @param {Function} callback - callback
 */
exports.cleanCollections = function(dbName, collections, callback) {

    var uri = config[dbName].uri;
    var serverId = getServerId();
    var conditions = serverId 
        ? {serverid: serverId} 
        : {};

    MongoClient.connect(uri, function(err, db) {
        assert.isNull(err, `unexpected error, ${uri}, ${err}`);

        async.forEachOf(collections, function(collection, collectionName, callback) 
        {
            db
            .collection(collectionName)
            .deleteMany(conditions, function(err, result) {
                assert.isNull(err, `unexpected error, ${collectionName}, ${conditions}, ${err}`);
                callback();
            });
        }, function() {
            db.close();
            callback();
        });
    });
}

/**
 *  get the latest list of CDR from db
 *
 *  @param {Function} callback - returns result
 */
exports.getCdrs = function(callback) {

    var uri = config.cdr.uri;
    var serverId = getServerId();
    var conditions = serverId 
        ? {serverid: serverId} 
        : {};

    MongoClient.connect(uri, function(err, db) 
    {
        assert.isNull(err, `unexpected error, ${uri}, ${err}`);
        db
        .collection(config.cdr.collection)
        .find(conditions)
        .toArray(function(err, cdrs) {
            assert.isNull(err, `unexpected error, ${config.cdr.collection}, ${conditions}, ${err}`);
            db.close();
            callback(cdrs);
        });
    });
}

/**
 *  get the latest list of CEL from db
 *
 *  @param {Function} callback - returns result
 */
exports.getCels = function(callback) {

    var uri = config.cel.uri;
    var serverId = getServerId();
    var conditions = serverId 
        ? {serverid: serverId} 
        : {};

    MongoClient.connect(uri, function(err, db) 
    {
        assert.isNull(err, `unexpected error, ${uri}, ${err}`);
        db
        .collection(config.cel.collection)
        .find(conditions)
        .toArray(function(err, cels) {
            assert.isNull(err, `unexpected error, ${config.cel.collection}, ${conditions}, ${err}`);
            db.close();
            callback(cels);
        });
    });
}
