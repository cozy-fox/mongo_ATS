/*
 * MongoDB configuration engine
 *
 * Copyright: (c) 2015-2016 KINOSHITA minoru
 * License: GNU GENERAL PUBLIC LICENSE Version 2
 */

/*! \file
 *
 * \brief MongoDB configuration engine
 *
 * \author KINOSHITA minoru
 *
 * This is a realtime configuration engine for the MongoDB database
 * \ingroup resources
 *
 * Depends on the MongoDB library - https://www.mongodb.org
 *
 */

/*! \li \ref res_config_mongodb.c uses the configuration file \ref res_config_mongodb.conf
 * \addtogroup configuration_file Configuration Files
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

#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/config.h"
#include "asterisk/module.h"
#include "asterisk/lock.h"
#include "asterisk/utils.h"
#include "asterisk/threadstorage.h"
#include "asterisk/res_mongodb.h"

#define HANDLE_ID_AS_OID 1
#define BSON_UTF8_VALIDATE(utf8,allow_null) \
      bson_utf8_validate (utf8, (int) strlen (utf8), allow_null)

#define LOG_BSON_AS_JSON(level, fmt, bson, ...) { \
            size_t length;  \
            char *str = bson_as_json(bson, &length); \
            ast_log(level, fmt, str, ##__VA_ARGS__); \
            bson_free(str); \
        }

static const int MAXTOKENS = 3;
static const char NAME[] = "mongodb";
static const char CATEGORY[] = "config";
static const char CONFIG_FILE[] = "ast_mongo.conf";
static const char SERVERID[] = "serverid";

AST_MUTEX_DEFINE_STATIC(model_lock);
static mongoc_client_pool_t* dbpool = NULL;
static bson_t* models = NULL;
static bson_oid_t *serverid = NULL;
static void* apm_context = NULL;
static int apm_enabled = 0;

static int str_split(char* str, const char* delim, const char* tokens[] ) {
    char* token;
    char* saveptr;
    int count = 0;

    for(token = strtok_r(str, delim, &saveptr);
        token && count < MAXTOKENS;
        token = strtok_r(NULL, delim, &saveptr), count++)
    {
        tokens[count] = token;
    }
    return count;
}

static const char *key_mongo2asterisk(const char *key)
{
    return strcmp(key, "_id") == 0 ? "id" : key;
}

static const char *key_asterisk2mongo(const char *key)
{
    return strcmp(key, "id") == 0 ? "_id" : key;
}

/*!
 *  check if the specified string is integer
 *
 *  \param[in] value
 *  \param[out] result
 *  \retval true if it's an integer
 */
static bool is_integer(const char* value, long long* result)
{
    int len;
    long long dummy;
    long long* p = result ? result : &dummy;

    if (sscanf(value, "%Ld%n", p, &len) == 0)
        return false;
    if (value[len] != '\0')
        return false;
    return true;
}

/*!
 *  check if the specified string is real number
 *
 *  \param[in] value
 *  \param[out] result
 *  \retval true if it's a real number
 */
static bool is_real(const char* value, double* result)
{
    int len;
    double dummy;
    double* p = result ? result : &dummy;

    if (sscanf(value, "%lg%n", p, &len) == 0)
        return false;
    if (value[len] != '\0')
        return false;
    return true;
}

/*!
 *  check if the specified string is bool
 *
 *  \param[in] value
 *  \param[out] result
 *  \retval true if it's a real number
 */
static bool is_bool(const char* value, bool* result) {
    bool dummy;
    bool* p = result ? result : &dummy;

    if (strcmp(value, "true") == 0) {
        *p = true;
        return true;
    }
    if (strcmp(value, "false") == 0) {
        *p = false;
        return true;
    }
    return false;
}

/*!
 *  assume the specified src doesn't have any escaping letters for mongo such as \, ', ".
 *
 *  \retval a pointer same as dst.
 */
static const char* strcopy(const char* src, char* dst, int size)
{
    char* p = dst;
    int escaping  = 0;
    int i;
    for (i = 0; *src != '\0' && i < (size-1); ) {
        int c = *src++;
        if (escaping) {
            *p++ = c;
            ++i;
            escaping = 0;
        }
        else if (c == '%')
            break;
        else if (c == '\\')
            escaping = 1;
        else {
            *p++ = c;
            ++i;
        }
    }
    if (i == (size-1))
        ast_log(LOG_WARNING, "size of dst is not enough.\n");
    *p = '\0';
    return (const char*)dst;
}

/*!
 * \brief   make a condition to query
 * \param   sql     is pattern for sql
 * \retval  a bson to query as follows;
 *      sql patern      generated bson to query
 *      ----------      --------------------------------------
 *      %               { $exists: true, $not: { $size: 0} } }
 *      %patern%        { $regex: "patern" }
 *      patern%         { $regex: "^patern" }
 *      %patern         { $regex: "patern$" }
 *      any other       NULL
 */
static const bson_t* make_condition(const char* sql)
{
    bson_t* condition = NULL;
    char patern[1020];
    char tmp[1024];
    char head = *sql;
    char tail = *(sql + strlen(sql) - 1);

    if (strcmp(sql, "%") == 0) {
        const char* json = "{ \"$exists\": true, \"$not\": {\"$size\": 0}}";
        bson_error_t error;
        condition = bson_new_from_json((const uint8_t*)json, -1, &error);
        if (!condition)
            ast_log(LOG_ERROR, "cannot generated condition from \"%s\", %d.%d:%s\n", json, error.domain, error.code, error.message);
    }
    else if (head == '%' && tail == '%') {
        strcopy(sql+1, patern, sizeof(patern)-1);
        snprintf(tmp, sizeof(tmp), "%s", patern);
        condition = bson_new();
        BSON_APPEND_UTF8(condition, "$regex", tmp);
    }
    else if (head == '%') {
        strcopy(sql+1, patern, sizeof(patern)-1);
        snprintf(tmp, sizeof(tmp), "%s$", patern);
        condition = bson_new();
        BSON_APPEND_UTF8(condition, "$regex", tmp);
    }
    else if (tail == '%') {
        strcopy(sql, patern, sizeof(patern));
        snprintf(tmp, sizeof(tmp), "^%s", patern);
        condition = bson_new();
        BSON_APPEND_UTF8(condition, "$regex", tmp);
    }
    else {
        ast_log(LOG_WARNING, "not supported condition, \"%s\"\n", sql);
    }

    if (condition) {
        // LOG_BSON_AS_JSON(LOG_DEBUG, "generated condition is \"%s\"\n", condition);
    }
    else
        ast_log(LOG_WARNING, "no condition generated\n");

    return (const bson_t*)condition;
}

/*!
 * \brief make a query
 * \param fields
 * \param orderby
 * \retval  a bson object to query,
 * \retval  NULL if something wrong.
 */
static bson_t *make_query(const struct ast_variable *fields, const char *orderby)
{
    bson_t *root = NULL;
    bson_t *query = NULL;
    bson_t *order = NULL;

    do {
        bool err;

        query = serverid ? BCON_NEW(SERVERID, BCON_OID(serverid)) : bson_new();
        order = orderby ? BCON_NEW(key_asterisk2mongo(orderby), BCON_DOUBLE(1)) : bson_new();

        for(err = false; fields && !err; fields = fields->next) {
            const bson_t *condition = NULL;
            const char *tokens[MAXTOKENS];
            char buf[1024];
            int count;
            long long ll_number;

            if (strlen(fields->name) >= (sizeof(buf) - 1)) {
                ast_log(LOG_WARNING, "too long key, \"%s\".\n", fields->name);
                continue;
            }
            strcpy(buf, fields->name);
            count = str_split(buf, " ", tokens);
            err = true;

            switch(count) {
                case 1:
#ifdef HANDLE_ID_AS_OID
                    if ((strcmp(fields->name, "id") == 0)
                    &&  bson_oid_is_valid(fields->value, strlen(fields->value))) {
                        bson_oid_t oid;
                        bson_oid_init_from_string(&oid, fields->value);
                        err = !BSON_APPEND_OID(query, "_id", &oid);
                    }
                    else
#endif
                        err = !BSON_APPEND_UTF8(query, key_asterisk2mongo(fields->name), fields->value);
                    break;
                case 2:
                    if (!strcasecmp(tokens[1], "LIKE")) {
                        condition = make_condition(fields->value);
                    }
                    else if (!strcasecmp(tokens[1], "!=")) {
                        // {
                        //     tokens[0]: {
                        //         "$exists" : true,
                        //         "$ne" : value
                        //     }
                        // }
                        condition = BCON_NEW(
                            "$exists", BCON_BOOL(1),
                            "$ne", BCON_UTF8(fields->value)
                        );
                    }
                    else if (!strcasecmp(tokens[1], ">")) {
                        // {
                        //     tokens[0]: {
                        //         "$gt" : value
                        //     }
                        // }
                        if (is_integer(fields->value, &ll_number))
                            condition = BCON_NEW("$gt", BCON_INT64(ll_number));
                        else
                            condition = BCON_NEW("$gt", BCON_UTF8(fields->value));
                    }
                    else if (!strcasecmp(tokens[1], "<=")) {
                        // {
                        //     tokens[0]: {
                        //         "$lte" : value
                        //     }
                        // }
                        if (is_integer(fields->value, &ll_number))
                            condition = BCON_NEW("$lte", BCON_INT64(ll_number));
                        else
                            condition = BCON_NEW("$lte", BCON_UTF8(fields->value));
                    }
                    else {
                        ast_log(LOG_WARNING, "unexpected operator \"%s\" of \"%s\" \"%s\".\n", tokens[1], fields->name, fields->value);
                        break;
                    }
                    if (!condition) {
                        ast_log(LOG_ERROR, "something wrong.\n");
                        break;
                    }

                    err = !BSON_APPEND_DOCUMENT(query, key_asterisk2mongo(tokens[0]), condition);

                    break;
                default:
                    ast_log(LOG_WARNING, "not handled, name=%s, value=%s.\n", fields->name, fields->value);
            }
            if (condition)
                bson_destroy((bson_t*)condition);
            else if (count > 1) {
                ast_log(LOG_ERROR, "something wrong.\n");
                break;
            }
        }
        if (err) {
            ast_log(LOG_ERROR, "something wrong.\n");
            break;
        }
        root = BCON_NEW("$query", BCON_DOCUMENT(query),
                        "$orderby", BCON_DOCUMENT(order));
        if (!root) {    // current BCON_NEW might not return any error such as NULL...
            ast_log(LOG_WARNING, "not enough memory\n");
            break;
        }
    } while(0);
    if (query)
        bson_destroy(query);
    if (order)
        bson_destroy(order);
    // if (root) {
    //     LOG_BSON_AS_JSON(LOG_DEBUG, "generated query is %s\n", root);
    // }
    return root;
}

/*!
 * \brief   check if the models library has specified collection.
 * \param   collection  is name of model to be retrieved.
 * \retval  true if the library poses the specified collection or any error,
 * \retval  false if not exist.
 */
static bool model_check(const char* collection)
{
    bson_iter_t iter;
    return bson_iter_init(&iter, models) && bson_iter_find(&iter, collection);
}

/*!
 * \param[in]   model_name  is name of model to be retrieved.
 * \param[in]   property
 * \param[in]   value
 * \retval  bson type
 */
static bson_type_t model_get_btype(const char* model_name, const char* property, const char* value)
{
    bson_type_t btype = BSON_TYPE_UNDEFINED;
    bson_iter_t iroot;
    bson_iter_t imodel;

    ast_mutex_lock(&model_lock);
    do {
        if (value) {
            if (is_bool(value, NULL))
                btype = BSON_TYPE_BOOL;
            else if (is_real(value, NULL))
                btype = BSON_TYPE_DOUBLE;
            else
                btype = BSON_TYPE_UTF8;
        }
        if (model_check(model_name) &&
            bson_iter_init_find (&iroot, models, model_name) &&
            BSON_ITER_HOLDS_DOCUMENT (&iroot) &&
            bson_iter_recurse (&iroot, &imodel) &&
            bson_iter_find(&imodel, property))
        {
            btype = (bson_type_t)bson_iter_as_int64(&imodel);
        }
    } while(0);
    ast_mutex_unlock(&model_lock);
    return btype;
}

static void model_register(const char *collection, const bson_t *model)
{
    ast_mutex_lock(&model_lock);
    do {
        if (model_check(collection))
            ast_log(LOG_DEBUG, "%s already registered\n", collection);
        else if (!BSON_APPEND_DOCUMENT(models, collection, model))
            ast_log(LOG_ERROR, "cannot register %s\n", collection);
        else {
            LOG_BSON_AS_JSON(LOG_DEBUG, "models is \"%s\"\n", models);
        }
    } while(0);
    ast_mutex_unlock(&model_lock);
}

static bson_type_t rtype2btype (require_type rtype)
{
    bson_type_t btype;
    switch(rtype) {
        case RQ_INTEGER1:
        case RQ_UINTEGER1:
        case RQ_INTEGER2:
        case RQ_UINTEGER2:
        case RQ_INTEGER3:
        case RQ_UINTEGER3:
        case RQ_INTEGER4:
        case RQ_UINTEGER4:
        case RQ_INTEGER8:
        case RQ_UINTEGER8:
        case RQ_FLOAT:
            btype = BSON_TYPE_DOUBLE;
            break;
        case RQ_DATE:
        case RQ_DATETIME:
        case RQ_CHAR:
            btype = BSON_TYPE_UTF8;
            break;
        default:
            ast_log(LOG_ERROR, "unexpected require type %d\n", rtype);
            btype = BSON_TYPE_UNDEFINED;
    }
    return btype;
}

/*!
 *  Make a document from key-value list
 *
 *  \param[in]  table
 *  \param[in]  fields
 *  \param[out] doc
 *  \retval  true if success
*/
static bool fields2doc(const char* table, const struct ast_variable *fields, bson_t *doc)
{
    bool err;
    const char* key;

    for (err = false; fields && !err; fields = fields->next) {
        bson_type_t btype;

        if (strlen(fields->value) == 0)
            continue;
        key = key_asterisk2mongo(fields->name);
        btype = model_get_btype(table, key, fields->value);
        switch(btype) {
            case BSON_TYPE_UTF8:
                err = !BSON_APPEND_UTF8(doc, key, fields->value);
                break;
            case BSON_TYPE_BOOL:
                err = !BSON_APPEND_BOOL(doc, key,
                    strcmp(fields->value, "true") ? false : true);
                break;
            case BSON_TYPE_INT32:
                err = !BSON_APPEND_INT32(doc, key, atol(fields->value));
                break;
            case BSON_TYPE_INT64:
                err = !BSON_APPEND_INT64(doc, key, atoll(fields->value));
                break;
            case BSON_TYPE_DOUBLE:
                err = !BSON_APPEND_DOUBLE(doc, key, atof(fields->value));
                break;
            default:
                ast_log(LOG_WARNING, "unexpected data type: key=%s, value=%s\n", key, fields->value);
                break;
        }
    }
    return !err;
}

/*!
 *  Get a value from a document
 *
 *  \param[in,out]  iter    is a bson iterator of a document
 *  \param[out]     key     is stored pointer to key name of value
 *  \param[out]     value   is a buffer to be stored a string of value
 *  \param[in]      size    is size of buffer for value
 *  \retval  true if value is valid.
*/
static bool doc2value(bson_iter_t* iter, const char** key, char value[], int size)
{
    if (size < 25) {
        ast_log(LOG_ERROR, "size of value is too small\n");
        return false;
    }
    if (BSON_ITER_HOLDS_OID(iter)) {
        const bson_oid_t * oid;
        if (strcmp(bson_iter_key(iter), SERVERID) == 0) {
            // SERVERID is hidden property for application
            return false;
        }
        oid = bson_iter_oid(iter);
        bson_oid_to_string(oid, value);
    }
    else if (BSON_ITER_HOLDS_UTF8(iter)) {
        uint32_t length;
        const char* str = bson_iter_utf8(iter, &length);
        if (!bson_utf8_validate(str, length, false)) {
            ast_log(LOG_WARNING, "unexpected invalid bson found\n");
            return false;
        }
        snprintf(value, size, "%s", str);
    }
    else if (BSON_ITER_HOLDS_BOOL(iter)) {
        bool d = bson_iter_bool(iter);
        snprintf(value, size, "%s", d ? "true" : "false");
    }
    else if (BSON_ITER_HOLDS_INT32(iter)) {
        long d = bson_iter_int32(iter);
        snprintf(value, size, "%ld", d);
    }
    else if (BSON_ITER_HOLDS_INT64(iter)) {
        long long d = bson_iter_int64(iter);
        snprintf(value, size, "%Ld", d);
    }
    else if (BSON_ITER_HOLDS_DOUBLE(iter)) {
        double d = bson_iter_double(iter);
        snprintf(value, size, "%.10g", d);
    }
    else {
        // see http://api.mongodb.org/libbson/current/bson_iter_type.html
        ast_log(LOG_WARNING, "unexpected bson type, %x\n", bson_iter_type(iter));
        return false;
    }
    *key = key_mongo2asterisk(bson_iter_key(iter));
    return true;
}

/*!
 * \brief Update documents in collection that match selector.
 * \param[in] collection    is a mongoc_collection_t.
 * \param[in] selector      is a bson_t containing the query to match documents for updating.
 * \param[in] update        is a bson_t containing the update to perform.
 *
 * \retval number of rows affected
 * \retval -1 on failure
*/
static int _collection_update(
    mongoc_collection_t *collection, const bson_t *selector, const bson_t *update)
{
    int ret = -1;
    bson_t *cmd = NULL;
    bson_t *opts = NULL;
    bson_t *updates = NULL;
    bson_t array = BSON_INITIALIZER;
    bson_t reply = BSON_INITIALIZER;

    LOG_BSON_AS_JSON(LOG_DEBUG, "selector=%s\n", selector);
    LOG_BSON_AS_JSON(LOG_DEBUG, "update=%s\n", update);

    do {
        bson_error_t error;
        bson_iter_t iter;

        opts = bson_new();
        updates = BCON_NEW(
            "q", BCON_DOCUMENT(selector),
            "u", BCON_DOCUMENT(update),
            "multi", BCON_BOOL(true)
        );
        bson_append_document(&array, "0", -1, updates);
        cmd = BCON_NEW(
           "update", BCON_UTF8(mongoc_collection_get_name(collection)),
           "updates", BCON_ARRAY(&array)
        );

        if (!mongoc_collection_write_command_with_opts(
            collection, cmd, opts, &reply, &error))
        {
            ast_log(LOG_ERROR, "update failed, error=%s\n", error.message);
            LOG_BSON_AS_JSON(LOG_ERROR, "cmd=%s\n", cmd);
            break;
        }
        LOG_BSON_AS_JSON(LOG_DEBUG, "reply=%s\n", &reply);

        if (!bson_iter_init(&iter, &reply)
        || !bson_iter_find(&iter, "nModified")
        || !BSON_ITER_HOLDS_INT32(&iter)) {
            ast_log(LOG_ERROR, "no \"nModified\" field found.\n");
            break;
        }
        ret = bson_iter_int32(&iter);
    } while(0);

    bson_destroy(&reply);
    bson_destroy(&array);
    if (updates)
        bson_destroy(updates);
    if (opts)
        bson_destroy(opts);
    if (cmd)
        bson_destroy(cmd);
    return ret;
}

/*!
 * \brief Execute an SQL query and return ast_variable list
 * \param database  is name of database
 * \param table     is name of collection to find specified records
 * \param ap list containing one or more field/operator/value set.
 *
 * Select database and perform query on table, prepare the sql statement
 * Sub-in the values to the prepared statement and execute it. Return results
 * as a ast_variable list.
 *
 * \retval var on success
 * \retval NULL on failure
 *
 * \see http://api.mongodb.org/c/current/finding-document.html
*/
static struct ast_variable *realtime(const char *database, const char *table, const struct ast_variable *fields)
{
    struct ast_variable *var = NULL;
    mongoc_client_t *dbclient;
    mongoc_collection_t *collection = NULL;
    mongoc_cursor_t *cursor = NULL;
    const bson_t *doc = NULL;
    bson_t *query = NULL;

    if (!database || !table || !fields) {
        ast_log(LOG_ERROR, "not enough arguments\n");
        return NULL;
    }
    ast_log(LOG_DEBUG, "database=%s, table=%s.\n", database, table);

    if(dbpool == NULL) {
        ast_log(LOG_ERROR, "no connection pool\n");
        return NULL;
    }

    dbclient = mongoc_client_pool_pop(dbpool);
    if(dbclient == NULL) {
        ast_log(LOG_ERROR, "no client allocated\n");
        return NULL;
    }

    do {
        query = make_query(fields, NULL);
        if(query == NULL) {
            ast_log(LOG_ERROR, "cannot make a query to find\n");
            break;
        }
        LOG_BSON_AS_JSON(LOG_DEBUG, "query=%s, database=%s, table=%s\n", query, database, table);

        collection = mongoc_client_get_collection(dbclient, database, table);
        cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1, 0, query, NULL, NULL);
        if (!cursor) {
            LOG_BSON_AS_JSON(LOG_ERROR, "query failed with query=%s, database=%s, table=%s\n", query, database, table);
            break;
        }
        if (mongoc_cursor_next(cursor, &doc)) {
            bson_iter_t iter;
            const char* key;
            const char* value;
            char work[128];
            struct ast_variable *prev = NULL;

            LOG_BSON_AS_JSON(LOG_DEBUG, "query found %s\n", doc);

            if (!bson_iter_init(&iter, doc)) {
                ast_log(LOG_ERROR, "unexpected bson error!\n");
                break;
            }
            while (bson_iter_next(&iter)) {
                if (!doc2value(&iter, &key, work, sizeof(work)))
                    continue;
                value = work;
                if (prev) {
                    prev->next = ast_variable_new(key, value, "");
                    if (prev->next)
                        prev = prev->next;
                }
                else
                    prev = var = ast_variable_new(key, value, "");
            }
        }
    } while(0);

    if (doc)
        bson_destroy((bson_t *)doc);
    if (query)
        bson_destroy((bson_t *)query);
    if (cursor)
        mongoc_cursor_destroy(cursor);
    if (collection)
        mongoc_collection_destroy(collection);
    mongoc_client_pool_push(dbpool, dbclient);
    return var;
}

/*!
 * \brief Execute an Select query and return ast_config list
 * \param database  is name of database
 * \param table     is name of collection to find specified records
 * \param fields    is a list containing one or more field/operator/value set.
 *
 * Select database and preform query on table, prepare the sql statement
 * Sub-in the values to the prepared statement and execute it.
 * Execute this prepared query against MongoDB.
 * Return results as an ast_config variable.
 *
 * \retval var on success
 * \retval NULL on failure
 *
 * \see http://api.mongodb.org/c/current/finding-document.html
*/
static struct ast_config* realtime_multi(const char *database, const char *table, const struct ast_variable *fields)
{
    struct ast_config *cfg = NULL;
    struct ast_category *cat = NULL;
    struct ast_variable *var = NULL;
    mongoc_collection_t *collection = NULL;
    mongoc_cursor_t* cursor = NULL;
    mongoc_client_t* dbclient = NULL;
    const bson_t* doc = NULL;
    const bson_t* query = NULL;
    const char *initfield;
    char *op;

    if (!database || !table || !fields) {
        ast_log(LOG_ERROR, "not enough arguments\n");
        return NULL;
    }
    ast_log(LOG_DEBUG, "database=%s, table=%s.\n", database, table);

    if(dbpool == NULL) {
        ast_log(LOG_ERROR, "no connection pool\n");
        return NULL;
    }

    dbclient = mongoc_client_pool_pop(dbpool);
    if(dbclient == NULL) {
        ast_log(LOG_ERROR, "no client allocated\n");
        return NULL;
    }
    initfield = ast_strdupa(fields->name);
    if ((op = strchr(initfield, ' '))) {
        *op = '\0';
    }
    do {
        query = make_query(fields, initfield);
        if(query == NULL) {
            ast_log(LOG_ERROR, "cannot make a query to find\n");
            break;
        }

        cfg = ast_config_new();
        if (!cfg) {
            ast_log(LOG_WARNING, "out of memory!\n");
            break;
        }

        collection = mongoc_client_get_collection(dbclient, database, table);

        LOG_BSON_AS_JSON(LOG_DEBUG, "query=%s, database=%s, table=%s\n", query, database, table);

        cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 0, 0, query, NULL, NULL);
        if (!cursor) {
            LOG_BSON_AS_JSON(LOG_ERROR, "query failed with query=%s, database=%s, table=%s\n", query, database, table);
            break;
        }

        while (mongoc_cursor_next(cursor, &doc)) {
            bson_iter_t iter;
            const char* key;
            const char* value;
            char work[128];

            LOG_BSON_AS_JSON(LOG_DEBUG, "query found %s\n", doc);

            if (!bson_iter_init(&iter, doc)) {
                ast_log(LOG_ERROR, "unexpected bson error!\n");
                break;
            }
            cat = ast_category_new("", "", 99999);
            if (!cat) {
                ast_log(LOG_WARNING, "out of memory!\n");
                break;
            }
            while (bson_iter_next(&iter)) {
                if (!doc2value(&iter, &key, work, sizeof(work)))
                    continue;
                value = work;
                if (!strcmp(initfield, key))
                    ast_category_rename(cat, value);
                var = ast_variable_new(key, value, "");
                ast_variable_append(cat, var);
            }
            ast_category_append(cfg, cat);
        }
    } while(0);
    ast_log(LOG_DEBUG, "end of query.\n");

    if (query)
        bson_destroy((bson_t *)query);
    if (cursor)
        mongoc_cursor_destroy(cursor);
    if (collection)
        mongoc_collection_destroy(collection);
    mongoc_client_pool_push(dbpool, dbclient);
    return cfg;
}

/*!
 * \brief Execute an UPDATE query
 * \param database  is name of database
 * \param table     is name of collection to update records
 * \param keyfield  where clause field
 * \param lookup    value of field for where clause
 * \param fields    is a list containing one or more field/value set(s).
 *
 * Update a database table, prepare the sql statement using keyfield and lookup
 * control the number of records to change. All values to be changed are stored in ap list.
 * Sub-in the values to the prepared statement and execute it.
 *
 * \retval number of rows affected
 * \retval -1 on failure
*/
static int update(const char *database, const char *table, const char *keyfield, const char *lookup, const struct ast_variable *fields)
{
    int ret = -1;
    bson_t *query = NULL;
    bson_t *data = NULL;
    bson_t *update = NULL;
    mongoc_client_t *dbclient = NULL;
    mongoc_collection_t *collection = NULL;

    if (!database || !table || !keyfield || !lookup || !fields) {
        ast_log(LOG_ERROR, "not enough arguments\n");
        return -1;
    }
    ast_log(LOG_DEBUG, "database=%s, table=%s, keyfield=%s, lookup=%s.\n", database, table, keyfield, lookup);

    if(dbpool == NULL) {
        ast_log(LOG_ERROR, "no connection pool\n");
        return -1;
    }
    dbclient = mongoc_client_pool_pop(dbpool);
    if(dbclient == NULL) {
        ast_log(LOG_ERROR, "no client allocated\n");
        return -1;
    }

    do {
        query = serverid ? BCON_NEW(SERVERID, BCON_OID(serverid)) : bson_new();
        if (!query) {
            ast_log(LOG_ERROR, "not enough memory\n");
            break;
        }
        if (!BSON_APPEND_UTF8(query, key_asterisk2mongo(keyfield), lookup)) {
            ast_log(LOG_ERROR, "cannot make a query\n");
            break;
        }

        data = bson_new();
        if (!data) {
            ast_log(LOG_ERROR, "not enough memory\n");
            break;
        }
        update = bson_new();
        if (!update) {
            ast_log(LOG_ERROR, "not enough memory\n");
            break;
        }
        if (!fields2doc(table, fields, data)) {
            ast_log(LOG_ERROR, "cannot make data to update\n");
            break;
        }
        if (!BSON_APPEND_DOCUMENT(update, "$set", data)) {
            ast_log(LOG_ERROR,
                "cannot make data to update, database=%s, table=%s, keyfield=%s, lookup=%s\n",
                database, table, keyfield, lookup);
            break;
        }

        collection = mongoc_client_get_collection(dbclient, database, table);
        ret = _collection_update(collection, query, update);
    } while(0);

    if (data)
        bson_destroy((bson_t *)data);
    if (update)
        bson_destroy((bson_t *)update);
    if (query)
        bson_destroy((bson_t *)query);
    if (collection)
        mongoc_collection_destroy(collection);

    mongoc_client_pool_push(dbpool, dbclient);
    return ret;
}

/*!
 * \brief Callback for ast_realtime_require
 * \retval 0 Required fields met specified standards
 * \retval -1 One or more fields was missing or insufficient
 */
static int require(const char *database, const char *table, va_list ap)
{
    bson_t *model = bson_new();
    char *elm;

    if (!database || !table) {
        ast_log(LOG_ERROR, "not enough arguments\n");
        return -1;
    }
    ast_log(LOG_DEBUG, "database=%s, table=%s.\n", database, table);

    while ((elm = va_arg(ap, char *))) {
        int type = va_arg(ap, require_type);
        // int size =
        va_arg(ap, int);
        // ast_log(LOG_DEBUG, "elm=%s, type=%d, size=%d\n", elm, type, size);
        BSON_APPEND_INT64(model, elm, rtype2btype(type));
    }
    LOG_BSON_AS_JSON(LOG_DEBUG, "required model is \"%s\"\n", model);

    model_register(table, model);
    bson_destroy(model);
    return 0;
}

/*!
 * \brief Execute an UPDATE query
 * \param database  is name of database
 * \param table     is name of collection to update records
 * \param ap list containing one or more field/value set(s).
 *
 * Update a database table, preparing the sql statement from a list of
 * key/value pairs specified in ap.  The lookup pairs are specified first
 * and are separated from the update pairs by a sentinel value.
 * Sub-in the values to the prepared statement and execute it.
 *
 * \retval number of rows affected
 * \retval -1 on failure
*/
static int update2(const char *database, const char *table, const struct ast_variable *lookup_fields, const struct ast_variable *update_fields)
{
    int ret = -1;
    bson_t *query = NULL;
    bson_t *data = NULL;
    bson_t *update = NULL;
    mongoc_client_t *dbclient = NULL;
    mongoc_collection_t *collection = NULL;

    if (!database || !table || !lookup_fields || !update_fields) {
        ast_log(LOG_ERROR, "not enough arguments\n");
        return -1;
    }
    ast_log(LOG_DEBUG, "database=%s, table=%s\n", database, table);

    if(dbpool == NULL) {
        ast_log(LOG_ERROR, "no connection pool\n");
        return -1;
    }
    dbclient = mongoc_client_pool_pop(dbpool);
    if(dbclient == NULL) {
        ast_log(LOG_ERROR, "no client allocated\n");
        return -1;
    }

    do {
        query = serverid ? BCON_NEW(SERVERID, BCON_OID(serverid)) : bson_new();
        if (!query) {
            ast_log(LOG_ERROR, "not enough memory\n");
            break;
        }
        if (!fields2doc(table, lookup_fields, query)) {
            ast_log(LOG_ERROR, "cannot make data to update\n");
            break;
        }

        LOG_BSON_AS_JSON(LOG_DEBUG, "query=%s\n", query);

        data = bson_new();
        if (!data) {
            ast_log(LOG_ERROR, "not enough memory\n");
            break;
        }
        if (!fields2doc(table, update_fields, data)) {
            ast_log(LOG_ERROR, "cannot make data to update\n");
            break;
        }
        update = bson_new();
        if (!update) {
            ast_log(LOG_ERROR, "not enough memory\n");
            break;
        }
        if (!BSON_APPEND_DOCUMENT(update, "$set", data)) {
            ast_log(LOG_ERROR, "cannot make data to update, database=%s, table=%s\n",
                    database, table);
            break;
        }

        collection = mongoc_client_get_collection(dbclient, database, table);
        ret = _collection_update(collection, query, update);

    } while(0);

    if (data)
        bson_destroy((bson_t *)data);
    if (update)
        bson_destroy((bson_t *)update);
    if (query)
        bson_destroy((bson_t *)query);
    if (collection)
        mongoc_collection_destroy(collection);

    mongoc_client_pool_push(dbpool, dbclient);
    return ret;
}

/*!
 * \brief Execute an INSERT query
 * \param database
 * \param table
 * \param ap list containing one or more field/value set(s)
 *
 * Insert a new record into database table, prepare the sql statement.
 * All values to be changed are stored in ap list.
 * Sub-in the values to the prepared statement and execute it.
 *
 * \retval number of rows affected
 * \retval -1 on failure
*/
static int store(const char *database, const char *table, const struct ast_variable *fields)
{
    int ret = -1;
    bson_t *document = NULL;
    mongoc_client_t *dbclient = NULL;
    mongoc_collection_t *collection = NULL;

    if (!database || !table || !fields) {
        ast_log(LOG_ERROR, "not enough arguments\n");
        return -1;
    }
    ast_log(LOG_DEBUG, "database=%s, table=%s.\n", database, table);

    if(dbpool == NULL) {
        ast_log(LOG_ERROR, "no connection pool\n");
        return -1;
    }
    dbclient = mongoc_client_pool_pop(dbpool);
    if(dbclient == NULL) {
        ast_log(LOG_ERROR, "no client allocated\n");
        return -1;
    }

    do {
        bson_error_t error;

        document = serverid ? BCON_NEW(SERVERID, BCON_OID(serverid)) : bson_new();
        if (!document) {
            ast_log(LOG_ERROR, "not enough memory\n");
            break;
        }

        collection = mongoc_client_get_collection(dbclient, database, table);

        if (!fields2doc(table, fields, document)) {
            ast_log(LOG_ERROR, "cannot make a document to update\n");
            break;
        }

        LOG_BSON_AS_JSON(LOG_DEBUG, "document=%s\n", document);

        if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, document, NULL, &error)) {
            ast_log(LOG_ERROR, "store failed, error=%s\n", error.message);
            LOG_BSON_AS_JSON(LOG_ERROR, "document=%s\n", document);
            break;
        }

        ret = 1; // success
    } while(0);

    if (document)
        bson_destroy((bson_t *)document);
    if (collection)
        mongoc_collection_destroy(collection);
    mongoc_client_pool_push(dbpool, dbclient);
    return ret;
}

/*!
 * \brief Execute an DELETE query
 * \param database
 * \param table
 * \param keyfield where clause field
 * \param lookup value of field for where clause
 * \param ap list containing one or more field/value set(s)
 *
 * Delete a row from a database table, prepare the sql statement using keyfield and lookup
 * control the number of records to change. Additional params to match rows are stored in ap list.
 * Sub-in the values to the prepared statement and execute it.
 *
 * \retval number of rows affected
 * \retval -1 on failure
*/
static int destroy(const char *database, const char *table, const char *keyfield, const char *lookup, const struct ast_variable *fields)
{
    int ret = -1;
    bson_t *selector = NULL;
    mongoc_client_t *dbclient = NULL;
    mongoc_collection_t *collection = NULL;

    if (!database || !table || !keyfield || !lookup) {
        ast_log(LOG_ERROR, "not enough arguments\n");
        return -1;
    }
    ast_log(LOG_DEBUG, "database=%s, table=%s, keyfield=%s, lookup=%s.\n", database, table, keyfield, lookup);
    ast_log(LOG_DEBUG, "fields->name=%s, fields->value=%s.\n", fields?fields->name:"NULL", fields?fields->value:"NULL");

    if(dbpool == NULL) {
        ast_log(LOG_ERROR, "no connection pool\n");
        return -1;
    }
    dbclient = mongoc_client_pool_pop(dbpool);
    if(dbclient == NULL) {
        ast_log(LOG_ERROR, "no client allocated\n");
        return -1;
    }

    do {
        bson_error_t error;

        selector = serverid ? BCON_NEW(SERVERID, BCON_OID(serverid)) : bson_new();
        if (!selector) {
            ast_log(LOG_ERROR, "not enough memory\n");
            break;
        }
        if (!BSON_APPEND_UTF8(selector, key_asterisk2mongo(keyfield), lookup)) {
            ast_log(LOG_ERROR, "cannot make a query\n");
            break;
        }

        collection = mongoc_client_get_collection(dbclient, database, table);

        if (!mongoc_collection_remove(collection, MONGOC_REMOVE_SINGLE_REMOVE, selector, NULL, &error)) {
             ast_log(LOG_ERROR, "destroy failed, error=%s\n", error.message);
             break;
        }

        ret = 1; // success
    } while(0);

    if (selector)
        bson_destroy((bson_t *)selector);
    if (collection)
        mongoc_collection_destroy(collection);
    mongoc_client_pool_push(dbpool, dbclient);
    return ret;
}

static struct ast_config *load(
    const char *database, const char *table, const char *file, struct ast_config *cfg, struct ast_flags flags, const char *sugg_incl, const char *who_asked)
{
    struct ast_variable *new_v;
    struct ast_category *cur_cat;
    struct ast_flags loader_flags = { 0 };
    mongoc_collection_t *collection = NULL;
    mongoc_cursor_t* cursor = NULL;
    mongoc_client_t* dbclient = NULL;
    bson_t *query = NULL;
    const bson_t *doc = NULL;
    const bson_t *order = NULL;
    const bson_t *root = NULL;
    const bson_t *fields = NULL;
    const char *last_category = "";
    int last_cat_metric = -1;

    if (!database || !table || !file || !cfg || !who_asked) {
        ast_log(LOG_ERROR, "not enough arguments\n");
        return NULL;
    }
    if (!strcmp (file, CONFIG_FILE))
        return NULL;        /* cant configure myself with myself ! */
    if(dbpool == NULL) {
        ast_log(LOG_ERROR, "no connection pool\n");
        return NULL;
    }

    dbclient = mongoc_client_pool_pop(dbpool);
    if(dbclient == NULL) {
        ast_log(LOG_ERROR, "no client allocated\n");
        return NULL;
    }

    do {
        query = serverid ? BCON_NEW(SERVERID, BCON_OID(serverid)) : bson_new();
        if (!BSON_APPEND_UTF8(query, "filename", file)) {
            ast_log(LOG_ERROR, "unexpected bson error with filename=%s\n", file);
            break;
        }
        if (!BSON_APPEND_DOUBLE(query, "commented", 0)) {
            ast_log(LOG_ERROR, "unexpected bson error\n");
            break;
        }
        order = BCON_NEW(   "cat_metric", BCON_DOUBLE(-1),
                            "var_metric", BCON_DOUBLE(1),
                            "category", BCON_DOUBLE(1),
                            "var_name", BCON_DOUBLE(1));
        root = BCON_NEW(    "$query", BCON_DOCUMENT(query),
                            "$orderby", BCON_DOCUMENT(order));
        fields = BCON_NEW(  "cat_metric", BCON_DOUBLE(1),
                            "category", BCON_DOUBLE(1),
                            "var_name", BCON_DOUBLE(1),
                            "var_val", BCON_DOUBLE(1));

        LOG_BSON_AS_JSON(LOG_DEBUG, "query=%s\n", root);
        // LOG_BSON_AS_JSON(LOG_DEBUG, "fields=%s\n", fields);

        collection = mongoc_client_get_collection(dbclient, database, table);
        cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 0, 0, root, fields, NULL);
        if (!cursor) {
            LOG_BSON_AS_JSON(LOG_ERROR, "query failed with query=%s\n", root);
            LOG_BSON_AS_JSON(LOG_ERROR, "query failed with fields=%s\n", fields);
            break;
        }

        cur_cat = ast_config_get_current_category(cfg);

        while (mongoc_cursor_next(cursor, &doc)) {
            bson_iter_t iter;
            const char *var_name;
            const char *var_val;
            const char *category;
            int cat_metric;
            uint32_t length;

            LOG_BSON_AS_JSON(LOG_DEBUG, "query found %s\n", doc);

            if (!bson_iter_init(&iter, doc)) {
                ast_log(LOG_ERROR, "unexpected bson error!\n");
                break;
            }

            if(!bson_iter_find(&iter, "cat_metric")) {
                ast_log(LOG_ERROR, "no cat_metric found!\n");
                break;
            }
            cat_metric = (int)bson_iter_double(&iter);

            if(!bson_iter_find(&iter, "category")) {
                ast_log(LOG_ERROR, "no category found!\n");
                break;
            }
            category = bson_iter_utf8(&iter, &length);
            if (!category) {
                ast_log(LOG_ERROR, "cannot read category.\n");
                break;
            }

            if(!bson_iter_find(&iter, "var_name")) {
                ast_log(LOG_ERROR, "no var_name found!\n");
                break;
            }
            var_name = bson_iter_utf8(&iter, &length);
            if (!var_name) {
                ast_log(LOG_ERROR, "cannot read var_name.\n");
                break;
            }

            if(!bson_iter_find(&iter, "var_val")) {
                ast_log(LOG_ERROR, "no var_val found!\n");
                break;
            }
            var_val = bson_iter_utf8(&iter, &length);
            if (!var_val) {
                ast_log(LOG_ERROR, "cannot read var_val.\n");
                break;
            }

            if (!strcmp (var_val, "#include")) {
                if (!ast_config_internal_load(var_val, cfg, loader_flags, "", who_asked)) {
                    ast_log(LOG_DEBUG, "ended with who_asked=%s\n", who_asked);
                    break;
                }
                ast_log(LOG_DEBUG, "#include ignored, who_asked=%s\n", who_asked);
                continue;
            }

            if (strcmp(last_category, category) || last_cat_metric != cat_metric) {
                cur_cat = ast_category_new(category, "", 99999);
                if (!cur_cat) {
                    ast_log(LOG_WARNING, "Out of memory!\n");
                    break;
                }
                last_category = category;
                last_cat_metric = cat_metric;
                ast_category_append(cfg, cur_cat);
            }

            new_v = ast_variable_new(var_name, var_val, "");
            ast_variable_append(cur_cat, new_v);
        }
    } while(0);

    if (doc)
        bson_destroy((bson_t *)doc);
    if (fields)
        bson_destroy((bson_t *)fields);
    if (query)
        bson_destroy((bson_t *)query);
    if (order)
        bson_destroy((bson_t *)order);
    if (root)
        bson_destroy((bson_t *)root);
    if (cursor)
        mongoc_cursor_destroy(cursor);
    if (collection)
        mongoc_collection_destroy(collection);
    mongoc_client_pool_push(dbpool, dbclient);
    return cfg;
}

/*!
 * \brief Callback for clearing any cached info
 * \note We don't currently cache anything
 * \retval 0 If any cache was purged
 * \retval -1 If no cache was found
 */
static int unload(const char *a, const char *b)
{
    ast_log(LOG_DEBUG, "database=%s, table=%s\n", a, b);
    /* We currently do no caching */
    return -1;
}


static int config(int reload)
{
    int res = -1;
    struct ast_config *cfg = NULL;
    mongoc_uri_t *uri = NULL;
    ast_log(LOG_DEBUG, "reload=%d\n", reload);

    do {
        const char *tmp;
        struct ast_variable *var;
        struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };

        cfg = ast_config_load(CONFIG_FILE, config_flags);
        if (!cfg || cfg == CONFIG_STATUS_FILEINVALID) {
            ast_log(LOG_WARNING, "unable to load %s\n", CONFIG_FILE);
            res = AST_MODULE_LOAD_DECLINE;
            break;
        } else if (cfg == CONFIG_STATUS_FILEUNCHANGED)
            break;

        var = ast_variable_browse(cfg, CATEGORY);
        if (!var) {
            ast_log(LOG_WARNING, "no category %s specified.\n", CATEGORY);
            break;
        }

        if ((tmp = ast_variable_retrieve(cfg, CATEGORY, "uri")) == NULL) {
            ast_log(LOG_WARNING, "no uri specified.\n");
            break;
        }
        uri = mongoc_uri_new(tmp);
        if (uri == NULL) {
            ast_log(LOG_ERROR, "parsing uri error, %s\n", tmp);
            break;
        }

        if ((tmp = ast_variable_retrieve(cfg, CATEGORY, "apm"))
        && (sscanf(tmp, "%u", &apm_enabled) != 1)) {
           ast_log(LOG_WARNING, "apm must be a 0|1, not '%s'\n", tmp);
           apm_enabled = 0;
        }
        if (apm_context)
            ast_mongo_apm_stop(apm_context);

        if (dbpool)
            mongoc_client_pool_destroy(dbpool);
        dbpool = mongoc_client_pool_new(uri);
        if (dbpool == NULL) {
            ast_log(LOG_ERROR, "cannot make a connection pool for MongoDB\n");
            break;
        }

        if (apm_enabled)
            apm_context = ast_mongo_apm_start(dbpool);

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

        res = 0; // success
    } while (0);

    if (uri)
       mongoc_uri_destroy(uri);
    if (cfg && cfg != CONFIG_STATUS_FILEUNCHANGED && cfg != CONFIG_STATUS_FILEINVALID) {
        ast_config_destroy(cfg);
    }

    models = bson_new();

    return res;
}

static struct ast_config_engine mongodb_engine = {
    .name = (char *)NAME,
    .load_func = load,
    .realtime_func = realtime,
    .realtime_multi_func = realtime_multi,
    .store_func = store,
    .destroy_func = destroy,
    .update_func = update,
    .update2_func = update2,
    .require_func = require,
    .unload_func = unload,
};

static int unload_module(void)
{
    ast_config_engine_deregister(&mongodb_engine);
    if (models)
        bson_destroy(models);
    if (apm_context)
        ast_mongo_apm_stop(apm_context);
    if (dbpool)
        mongoc_client_pool_destroy(dbpool);
    ast_log(LOG_DEBUG, "unloaded.\n");
    return 0;
}

static int load_module(void)
{
    if (config(0))
        return AST_MODULE_LOAD_DECLINE;
    ast_config_engine_register(&mongodb_engine);
    return 0;
}

static int reload_module(void)
{
    return config(1);
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "Realtime MongoDB configuration",
    .support_level = AST_MODULE_SUPPORT_CORE,
    .load = load_module,
    .unload = unload_module,
    .reload = reload_module,
    .load_pri = AST_MODPRI_REALTIME_DRIVER,
);
