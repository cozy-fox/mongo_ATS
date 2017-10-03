 /*
  * Asterisk -- An open source telephony toolkit.
  *
  * Copyright (C) 2015 Catalin [catacs] Stanciu <catacsdev@gmail.com>
  *
  * Catalin Stanciu - adapted to MongoDB backend, from:
  * Steve Murphy <murf@digium.com>
  * Adapted from the PostgreSQL CEL logger
  *
  *
  * Modified March 2016
  * Catalin Stanciu <catacsdev@gmail.com>
  *
  * See http://www.asterisk.org for more information about
  * the Asterisk project. Please do not directly contact
  * any of the maintainers of this project for assistance;
  * the project provides a web site, mailing lists and IRC
  * channels for your use.
  *
  * This program is free software, distributed under the terms of
  * the GNU General Public License Version 2. See the LICENSE file
  * at the top of the source tree.
  */

 /*! \file
  *
  * \brief MongoDB CEL logger
  *
  * \author Catalin Stanciu <catacsdev@gmail.com>
  * MongoDB https://www.mongodb.org/
  *
  * See also
  * \arg \ref Config_cel
  * MongoDB https://www.mongodb.org/
  * \ingroup cel_drivers
  */

/*** MODULEINFO
    <depend>res_mongodb</depend>
    <depend>mongoc</depend>
    <depend>bson</depend>
    <support_level>extended</support_level>
 ***/

#include "asterisk.h"
#ifdef ASTERISK_REGISTER_FILE   /* deprecated from 15.0.0 */
ASTERISK_REGISTER_FILE()
#endif

#include "asterisk/config.h"
#include "asterisk/options.h"
#include "asterisk/channel.h"
#include "asterisk/cel.h"
#include "asterisk/module.h"
#include "asterisk/logger.h"
#include "asterisk/res_mongodb.h"

// #define DATE_FORMAT "%Y-%m-%d %T.%6q"

static const char NAME[] = "cel_mongodb";
static const char CATEGORY[] = "mongodb";
static const char URI[] = "uri";
static const char DATABSE[] = "database";
static const char COLLECTION[] = "collection";
static const char SERVERID[] = "serverid";
static const char CONFIG_FILE[] = "cel_mongodb.conf";

enum {
    CONFIG_REGISTERED = 1 << 0,
};

static struct ast_flags config = { 0 };
static char *dbname = NULL;
static char *dbcollection = NULL;
static mongoc_client_pool_t *dbpool = NULL;
static bson_oid_t *serverid = NULL;

static void mongodb_log(struct ast_event *event)
{
    bson_t *doc = NULL;
    mongoc_collection_t *collection = NULL;
    const char *name;
//    struct ast_tm tm;
//    char timestr[128];

    if(dbpool == NULL) {
        ast_log(LOG_ERROR, "unexpected error, no connection pool\n");
        return;
    }

    mongoc_client_t *dbclient = mongoc_client_pool_pop(dbpool);
    if(dbclient == NULL) {
        ast_log(LOG_ERROR, "unexpected error, no client allocated\n");
        return;
    }

    struct ast_cel_event_record record = {
    	.version = AST_CEL_EVENT_RECORD_VERSION,
    };

    if (ast_cel_fill_record(event, &record)) {
        ast_log(LOG_ERROR, "unexpected error, failed to extract event data\n");    	    
	return;
    }
    /* Handle user define events */
    name = record.event_name;
    if (record.event_type == AST_CEL_USER_DEFINED) {
	name = record.user_defined_name;
    }
    
//  ast_localtime(&record.event_time, &tm, NULL);
//  ast_strftime(timestr, sizeof(timestr), DATE_FORMAT, &tm);
    
    do {
        bson_error_t error;

        doc = bson_new();
        if(doc == NULL) {
            ast_log(LOG_ERROR, "cannot make a document\n");
            break;
        }
        mongoc_collection_t *collection = mongoc_client_get_collection(dbclient, dbname, dbcollection);
        if(collection == NULL) {
            ast_log(LOG_ERROR, "cannot get such a collection, %s, %s\n", dbname, dbcollection);
            break;
        }
        BSON_APPEND_INT32(doc, "eventtype", record.event_type);
        BSON_APPEND_UTF8(doc, "eventname", name);
        BSON_APPEND_UTF8(doc, "cid_name", record.caller_id_name);
        BSON_APPEND_UTF8(doc, "cid_num", record.caller_id_num);
        BSON_APPEND_UTF8(doc, "cid_ani", record.caller_id_ani);
        BSON_APPEND_UTF8(doc, "cid_rdnis", record.caller_id_rdnis);
        BSON_APPEND_UTF8(doc, "cid_dnid", record.caller_id_dnid);
        BSON_APPEND_UTF8(doc, "exten", record.extension);
        BSON_APPEND_UTF8(doc, "context", record.context);
        BSON_APPEND_UTF8(doc, "channame", record.channel_name);
        BSON_APPEND_UTF8(doc, "appname", record.application_name);
        BSON_APPEND_UTF8(doc, "appdata", record.application_data);
        BSON_APPEND_UTF8(doc, "accountcode", record.account_code);
        BSON_APPEND_UTF8(doc, "peeraccount", record.peer_account);
        BSON_APPEND_UTF8(doc, "uniqueid", record.unique_id);
        BSON_APPEND_UTF8(doc, "linkedid", record.linked_id);
        BSON_APPEND_UTF8(doc, "userfield", record.user_field);
        BSON_APPEND_UTF8(doc, "peer", record.peer);
        BSON_APPEND_UTF8(doc, "extra", record.extra);
        BSON_APPEND_TIMEVAL(doc, "eventtime", &record.event_time);
        if (serverid)
            BSON_APPEND_OID(doc, SERVERID, serverid);

        if(!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, doc, NULL, &error))
            ast_log(LOG_ERROR, "insertion failed, %s\n", error.message);

    } while(0);

    if (collection)
        mongoc_collection_destroy(collection);
    if (doc)
        bson_destroy(doc);
    mongoc_client_pool_push(dbpool, dbclient);
    return;
}

static int _load_module(int reload)
{
    int res = -1;
    struct ast_config *cfg = NULL;
    mongoc_uri_t *uri = NULL;

    do {
        const char *tmp;
        struct ast_variable *var;
        struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };

        cfg = ast_config_load(CONFIG_FILE, config_flags);
        if (!cfg || cfg == CONFIG_STATUS_FILEINVALID) {
            ast_log(LOG_WARNING, "unable to load config file=%s\n", CONFIG_FILE);
            res = AST_MODULE_LOAD_DECLINE;
            break;
        } 
        else if (cfg == CONFIG_STATUS_FILEUNCHANGED)
            break;

        var = ast_variable_browse(cfg, CATEGORY);
        if (!var) {
            ast_log(LOG_WARNING, "no category specified.\n");
            break;
        }

        if ((tmp = ast_variable_retrieve(cfg, CATEGORY, URI)) == NULL) {
            ast_log(LOG_WARNING, "no uri specified.\n");
            break;
        }
        uri = mongoc_uri_new(tmp);
        if (uri == NULL) {
            ast_log(LOG_ERROR, "parsing uri error, %s\n", tmp);
            break;
        }

        if ((tmp = ast_variable_retrieve(cfg, CATEGORY, DATABSE)) == NULL) {
            ast_log(LOG_WARNING, "no database specified.\n");
            break;
        }
        if (dbname)
            ast_free(dbname);
        dbname = ast_strdup(tmp);
        if (dbname == NULL) {
            ast_log(LOG_ERROR, "not enough memory for dbname\n");
            break;
        }

        if ((tmp = ast_variable_retrieve(cfg, CATEGORY, COLLECTION)) == NULL) {
            ast_log(LOG_WARNING, "no collection specified.\n");
            break;
        }
        if (dbcollection)
            ast_free(dbcollection);
        dbcollection = ast_strdup(tmp);
        if (dbcollection == NULL) {
            ast_log(LOG_ERROR, "not enough memory for dbcollection\n");
            break;
        }

        if (ast_test_flag(&config, CONFIG_REGISTERED)){
            ast_cel_backend_unregister(NAME);
            ast_clear_flag(&config, CONFIG_REGISTERED);
        }
        
        if (!ast_test_flag(&config, CONFIG_REGISTERED)) {
            res = ast_cel_backend_register(NAME, mongodb_log);
            if (res) {
                ast_log(LOG_ERROR, "unable to register CEL handling\n");
                break;
            }
            ast_set_flag(&config, CONFIG_REGISTERED);
        }

        if ((tmp = ast_variable_retrieve(cfg, CATEGORY, SERVERID)) != NULL) {
            if (!bson_oid_is_valid (tmp, strlen(tmp))) {
                ast_log(LOG_ERROR, "invalid server id specified.\n");
                break;
            }
            serverid = ast_malloc(sizeof(bson_oid_t));
            if (serverid == NULL) {
                ast_log(LOG_ERROR, "not enough memory\n");
                break;
            }
            bson_oid_init_from_string(serverid, tmp);
        }

        if (dbpool)
            mongoc_client_pool_destroy(dbpool);
        dbpool = mongoc_client_pool_new(uri);
        if (dbpool == NULL) {
            ast_log(LOG_ERROR, "cannot make a connection pool for MongoDB\n");
            break;
        }

        res = 0; // suceess
    } while (0);

    if (uri)
       mongoc_uri_destroy(uri);
        
    if (cfg && cfg != CONFIG_STATUS_FILEUNCHANGED && cfg != CONFIG_STATUS_FILEINVALID)
        ast_config_destroy(cfg);        

    return res;
}

static int load_module(void)
{
	return _load_module(0);
}

static int unload_module(void)
{
    if (ast_cel_backend_unregister(NAME))
        return -1;
    if (dbname)
        ast_free(dbname);
    if (dbcollection)
        ast_free(dbcollection);
    if (dbpool)
        mongoc_client_pool_destroy(dbpool);
    return 0;
}

static int reload(void)
{
	int res;
	res = _load_module(1);	
	return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "MongoDB CEL Backend",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.reload = reload,
);
