/*
 * MongoDB CDR Backend
 *
 * Copyright: (c) 2015-2016 KINOSHITA minoru
 * License: GNU GENERAL PUBLIC LICENSE Version 2
 */

/*!
 * \file
 * \brief MongoDB CDR Backend
 *
 * \author KINOSHITA minoru
 *
 * See also:
 * \arg http://www.mongodb.org
 * \arg \ref Config_cdr
 * \ingroup cdr_drivers
 */

/*! \li \ref cdr_mongodb.c uses the configuration file \ref cdr_mongodb.conf
 * \addtogroup configuration_file Configuration Files
 */

/*!
 * \page cdr_mongodb.conf cdr_mongodb.conf
 * \verbinclude cdr_mongodb.conf.sample
 */

/*** MODULEINFO
    <depend>res_mongodb</depend>
    <depend>mongoc</depend>
    <depend>bson</depend>
    <support_level>extended</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/config.h"
#include "asterisk/channel.h"
#include "asterisk/cdr.h"
#include "asterisk/module.h"
#include "asterisk/res_mongodb.h"

static const char NAME[] = "cdr_mongodb";
static const char CATEGORY[] = "mongodb";
static const char URI[] = "uri";
static const char DATABSE[] = "database";
static const char COLLECTION[] = "collection";
static const char SERVERID[] = "serverid";
static const char CONFIG_FILE[] = "cdr_mongodb.conf";

enum {
    CONFIG_REGISTERED = 1 << 0,
};

static struct ast_flags config = { 0 };
static char *dbname = NULL;
static char *dbcollection = NULL;
static mongoc_client_pool_t *dbpool = NULL;
static bson_oid_t *serverid = NULL;

static int mongodb_log(struct ast_cdr *cdr)
{
    int ret = -1;
    bson_t *doc = NULL;
    mongoc_collection_t *collection = NULL;

    if(dbpool == NULL) {
        ast_log(LOG_ERROR, "unexpected error, no connection pool\n");
        return ret;
    }

    mongoc_client_t *dbclient = mongoc_client_pool_pop(dbpool);
    if(dbclient == NULL) {
        ast_log(LOG_ERROR, "unexpected error, no client allocated\n");
        return ret;
    }

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
        BSON_APPEND_UTF8(doc, "clid", cdr->clid);
        BSON_APPEND_UTF8(doc, "src", cdr->src);
        BSON_APPEND_UTF8(doc, "dst", cdr->dst);
        BSON_APPEND_UTF8(doc, "dcontext", cdr->dcontext);
        BSON_APPEND_UTF8(doc, "channel", cdr->channel);
        BSON_APPEND_UTF8(doc, "dstchannel", cdr->dstchannel);
        BSON_APPEND_UTF8(doc, "lastapp", cdr->lastapp);
        BSON_APPEND_UTF8(doc, "lastdata", cdr->lastdata);
        BSON_APPEND_UTF8(doc, "disposition", ast_cdr_disp2str(cdr->disposition));
        BSON_APPEND_UTF8(doc, "amaflags", ast_channel_amaflags2string(cdr->amaflags));
        BSON_APPEND_UTF8(doc, "accountcode", cdr->accountcode);
        BSON_APPEND_UTF8(doc, "uniqueid", cdr->uniqueid);
        BSON_APPEND_UTF8(doc, "userfield", cdr->userfield);
        BSON_APPEND_UTF8(doc, "peeraccount", cdr->peeraccount);
        BSON_APPEND_UTF8(doc, "linkedid", cdr->linkedid);
        BSON_APPEND_INT32(doc, "duration", cdr->duration);
        BSON_APPEND_INT32(doc, "billsec", cdr->billsec);
        BSON_APPEND_INT32(doc, "sequence", cdr->sequence);
        BSON_APPEND_TIMEVAL(doc, "start", &cdr->start);
        BSON_APPEND_TIMEVAL(doc, "answer", &cdr->answer);
        BSON_APPEND_TIMEVAL(doc, "end", &cdr->end);
        if (serverid)
            BSON_APPEND_OID(doc, SERVERID, serverid);

        if(!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, doc, NULL, &error))
            ast_log(LOG_ERROR, "insertion failed, %s\n", error.message);

        ret = 0; // success
    } while(0);

    if (collection)
        mongoc_collection_destroy(collection);
    if (doc)
        bson_destroy(doc);
    mongoc_client_pool_push(dbpool, dbclient);
    return ret;
}

static int mongodb_load_module(int reload)
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

        if (!ast_test_flag(&config, CONFIG_REGISTERED)) {
            res = ast_cdr_register(NAME, ast_module_info->description, mongodb_log);
            if (res) {
                ast_log(LOG_ERROR, "unable to register CDR handling\n");
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
    if (ast_test_flag(&config, CONFIG_REGISTERED) && (!cfg || dbname == NULL || dbcollection == NULL)) {
        ast_cdr_backend_suspend(NAME);
        ast_clear_flag(&config, CONFIG_REGISTERED);
    } 
    else
        ast_cdr_backend_unsuspend(NAME);
    if (cfg && cfg != CONFIG_STATUS_FILEUNCHANGED && cfg != CONFIG_STATUS_FILEINVALID)
        ast_config_destroy(cfg);
    return res;
}

static int load_module(void)
{
    return mongodb_load_module(0);
}

static int unload_module(void)
{
    if (ast_cdr_unregister(NAME))
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
    return mongodb_load_module(1);
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "MongoDB CDR Backend",
    .support_level = AST_MODULE_SUPPORT_EXTENDED,
    .load = load_module,
    .unload = unload_module,
    .reload = reload,
    .load_pri = AST_MODPRI_CDR_DRIVER,
);
