/*
 * MongoDB Common Resources
 *
 * Copyright: (c) 2015-2016 KINOSHITA minoru
 * License: GNU GENERAL PUBLIC LICENSE Version 2
 */

/*! \file
 *
 * \brief MongoDB Common Resource
 *
 * \author KINOSHITA minoru
 *
 */

/*! \li \ref res_mongodb.c uses the configuration file \ref ast_mongo.conf
 * \addtogroup configuration_file Configuration Files
 */

/*** MODULEINFO
    <depend>mongoc</depend>
    <depend>bson</depend>
    <support_level>extended</support_level>
 ***/


#include "asterisk.h"
#ifdef ASTERISK_REGISTER_FILE   /* deprecated from 15.0.0 */
ASTERISK_REGISTER_FILE()
#endif

#include "asterisk/module.h"
#include "asterisk/res_mongodb.h"
#include "asterisk/config.h"

/*** DOCUMENTATION
    <function name="MongoDB" language="en_US">
        <synopsis>
            ast_mongo common resource
        </synopsis>
        <description>
            This is the ast_mongo common resource which provides;
            1. functions to init and clean up mongoDB C Driver,
            2. handlers for Application Performance Monitoring (APM).
        </description>
    </function>
 ***/

static const char CATEGORY[] = "common";
static const char CONFIG_FILE[] = "ast_mongo.conf";

typedef struct {
    mongoc_apm_callbacks_t *callbacks;

    unsigned started;
    unsigned succeeded;
    unsigned failed;

    unsigned server_changed_events;
    unsigned server_opening_events;
    unsigned server_closed_events;
    unsigned topology_changed_events;
    unsigned topology_opening_events;
    unsigned topology_closed_events;
    unsigned heartbeat_started_events;
    unsigned heartbeat_succeeded_events;
    unsigned heartbeat_failed_events;
} apm_context_t;

// 0 = disable monitoring, 0 != enable monitoring
static unsigned apm_command_monitoring = 0;
static unsigned apm_sdam_monitoring = 0;
// -1 = disable
// 0 = MONGOC_LOG_LEVEL_ERROR
// ,...,
// 6 = MONGOC_LOG_LEVEL_TRACE
static int mongoc_log_level = -1;

static int log_level_mongoc2ast(mongoc_log_level_t mongoc_level) {
    switch(mongoc_level) {
        case MONGOC_LOG_LEVEL_ERROR:
            return __LOG_ERROR;
        case MONGOC_LOG_LEVEL_CRITICAL:
        case MONGOC_LOG_LEVEL_WARNING:
            return __LOG_WARNING;
        case MONGOC_LOG_LEVEL_MESSAGE:
        case MONGOC_LOG_LEVEL_INFO:
            return __LOG_NOTICE;
        case MONGOC_LOG_LEVEL_DEBUG:
            return __LOG_DEBUG;
        case MONGOC_LOG_LEVEL_TRACE:
            return __LOG_VERBOSE;
        default:
            return __LOG_ERROR;
    }
}

static void mongoc_log_handler(mongoc_log_level_t log_level,
           const char *log_domain,
           const char *message,
           void *user_data)
{
   /* smaller values are more important */
   if (log_level <= mongoc_log_level) {
       ast_log(log_level_mongoc2ast(log_level), log_domain, 0, "ast_mongo", "%s\n", message);
   }
}

static void apm_command_started(const mongoc_apm_command_started_t *event)
{
    apm_context_t* context = mongoc_apm_command_started_get_context(event);
    context->started++;

    if (apm_command_monitoring) {
        char *s = bson_as_canonical_extended_json(
            mongoc_apm_command_started_get_command(event), NULL);
        ast_log(LOG_NOTICE, "ast_mongo command %s started(%u) on %s, %s\n",
            mongoc_apm_command_started_get_command_name(event),
            context->started,
            mongoc_apm_command_started_get_host(event)->host,
            s);
        bson_free (s);
    }
}

static void apm_command_succeeded(const mongoc_apm_command_succeeded_t *event)
{
    apm_context_t* context = mongoc_apm_command_succeeded_get_context(event);
    context->succeeded++;

    if (apm_command_monitoring) {
        char *s = bson_as_canonical_extended_json(
            mongoc_apm_command_succeeded_get_reply(event), NULL);
        ast_log(LOG_NOTICE, "ast_mongo command %s succeeded(%u), %s\n",
            mongoc_apm_command_succeeded_get_command_name(event),
            context->succeeded,
            s);
        bson_free (s);
    }
}

static void apm_command_failed(const mongoc_apm_command_failed_t *event)
{
    apm_context_t* context = mongoc_apm_command_failed_get_context(event);
    context->failed++;

    if (apm_command_monitoring) {
        bson_error_t error;
        mongoc_apm_command_failed_get_error(event, &error);
        ast_log(LOG_WARNING, "ast_mongo command %s failed(%u), %s\n",
             mongoc_apm_command_failed_get_command_name(event),
             context->failed,
             error.message);
    }
}

static void apm_server_changed(const mongoc_apm_server_changed_t *event)
{
    apm_context_t* context = mongoc_apm_server_changed_get_context(event);
    context->server_changed_events++;

    if (apm_sdam_monitoring) {
        const mongoc_server_description_t *prev_sd
            = mongoc_apm_server_changed_get_previous_description (event);
        const mongoc_server_description_t *new_sd
            = mongoc_apm_server_changed_get_new_description(event);

        ast_log(LOG_NOTICE, "ast_mongo server changed(%u): %s %s -> %s\n",
            context->server_changed_events,
            mongoc_apm_server_changed_get_host(event)->host_and_port,
            mongoc_server_description_type(prev_sd),
            mongoc_server_description_type(new_sd));
    }
}

static void apm_server_opening(const mongoc_apm_server_opening_t *event)
{
    apm_context_t* context = mongoc_apm_server_opening_get_context(event);
    context->server_opening_events++;

    if (apm_sdam_monitoring) {
        ast_log(LOG_NOTICE, "ast_mongo server opening(%u): %s\n",
            context->server_opening_events,
            mongoc_apm_server_opening_get_host(event)->host_and_port);
    }
}

static void apm_server_closed(const mongoc_apm_server_closed_t *event)
{
    apm_context_t* context = mongoc_apm_server_closed_get_context(event);
    context->server_closed_events++;

    if (apm_sdam_monitoring) {
        ast_log(LOG_NOTICE, "ast_mongo server closed(%u): %s\n",
            context->server_closed_events,
            mongoc_apm_server_closed_get_host(event)->host_and_port);
    }
}

static void apm_topology_changed(const mongoc_apm_topology_changed_t *event)
{
    apm_context_t* context = mongoc_apm_topology_changed_get_context(event);
    context->topology_changed_events++;

    if (apm_sdam_monitoring) {
        size_t n_prev_sds;
        size_t n_new_sds;

        const mongoc_topology_description_t *prev_td
            = mongoc_apm_topology_changed_get_previous_description(event);
        mongoc_server_description_t **prev_sds
            = mongoc_topology_description_get_servers(prev_td, &n_prev_sds);

        const mongoc_topology_description_t *new_td
            = mongoc_apm_topology_changed_get_new_description(event);
        mongoc_server_description_t **new_sds
            = mongoc_topology_description_get_servers(new_td, &n_new_sds);

        ast_log(LOG_NOTICE, "ast_mongo topology changed(%u): %s -> %s\n",
            context->topology_changed_events,
            mongoc_topology_description_type(prev_td),
            mongoc_topology_description_type(new_td));

        {
            size_t i;
            if (n_prev_sds) {
                for (i = 0; i < n_prev_sds; i++) {
                    ast_log(LOG_NOTICE, "ast_mongo previous servers: %s %s\n",
                        mongoc_server_description_type(prev_sds[i]),
                        mongoc_server_description_host(prev_sds[i])->host_and_port);
                }
            }
            if (n_new_sds) {
                for (i = 0; i < n_new_sds; i++) {
                    ast_log(LOG_NOTICE, "ast_mongo new servers: %s %s\n",
                        mongoc_server_description_type(new_sds[i]),
                        mongoc_server_description_host(new_sds[i])->host_and_port);
                }
            }
        }

        mongoc_server_descriptions_destroy_all(prev_sds, n_prev_sds);
        mongoc_server_descriptions_destroy_all(new_sds, n_new_sds);
    }
}

static void apm_topology_opening(const mongoc_apm_topology_opening_t *event)
{
    apm_context_t* context = mongoc_apm_topology_opening_get_context(event);
    context->topology_opening_events++;

    if (apm_sdam_monitoring) {
        ast_log(LOG_NOTICE, "ast_mongo topology opening(%u)\n",
            context->topology_opening_events);
    }
}

static void apm_topology_closed(const mongoc_apm_topology_closed_t *event)
{
    apm_context_t* context = mongoc_apm_topology_closed_get_context(event);
    context->topology_closed_events++;

    if (apm_sdam_monitoring) {
        ast_log(LOG_NOTICE, "ast_mongo topology closed(%u)\n",
            context->topology_closed_events);
    }
}

static void apm_server_heartbeat_started(const mongoc_apm_server_heartbeat_started_t *event)
{
    apm_context_t* context = mongoc_apm_server_heartbeat_started_get_context(event);
    context->heartbeat_started_events++;

    if (apm_sdam_monitoring) {
        ast_log(LOG_NOTICE, "ast_mongo %s heartbeat started(%u)\n",
            mongoc_apm_server_heartbeat_started_get_host(event)->host_and_port,
            context->heartbeat_started_events);
    }
}

static void apm_server_heartbeat_succeeded(const mongoc_apm_server_heartbeat_succeeded_t *event)
{
    apm_context_t* context = mongoc_apm_server_heartbeat_succeeded_get_context(event);
    context->heartbeat_succeeded_events++;

    if (apm_sdam_monitoring) {
        char *reply = bson_as_canonical_extended_json(
            mongoc_apm_server_heartbeat_succeeded_get_reply(event), NULL);

        ast_log(LOG_NOTICE, "ast_mongo %s heartbeat succeeded(%u): %s\n",
            mongoc_apm_server_heartbeat_succeeded_get_host(event)->host_and_port,
            context->heartbeat_succeeded_events,
            reply);

        bson_free(reply);
    }
}

static void apm_server_heartbeat_failed(const mongoc_apm_server_heartbeat_failed_t *event)
{
    apm_context_t* context = mongoc_apm_server_heartbeat_failed_get_context(event);
    context->heartbeat_failed_events++;

    if (apm_sdam_monitoring) {
        bson_error_t error;
        mongoc_apm_server_heartbeat_failed_get_error(event, &error);

        ast_log(LOG_WARNING, "ast_mongo %s heartbeat failed(%u): %s\n",
            mongoc_apm_server_heartbeat_failed_get_host(event)->host_and_port,
            context->heartbeat_failed_events,
            error.message);
    }
}

void* ast_mongo_apm_start(mongoc_client_pool_t* pool)
{
    apm_context_t* context = ast_calloc(1, sizeof(apm_context_t));

    if (!context) {
        ast_log(LOG_ERROR, "not enough memory.\n");
        return NULL;
    }

    mongoc_client_pool_set_error_api(pool, 2);
    context->callbacks = mongoc_apm_callbacks_new();

    // for Command-Monitoring
    mongoc_apm_set_command_started_cb(context->callbacks, apm_command_started);
    mongoc_apm_set_command_succeeded_cb(context->callbacks, apm_command_succeeded);
    mongoc_apm_set_command_failed_cb(context->callbacks, apm_command_failed);

    // for SDAM Monitoring
    mongoc_apm_set_server_changed_cb(context->callbacks, apm_server_changed);
    mongoc_apm_set_server_opening_cb(context->callbacks, apm_server_opening);
    mongoc_apm_set_server_closed_cb(context->callbacks, apm_server_closed);
    mongoc_apm_set_topology_changed_cb(context->callbacks, apm_topology_changed);
    mongoc_apm_set_topology_opening_cb(context->callbacks, apm_topology_opening);
    mongoc_apm_set_topology_closed_cb(context->callbacks, apm_topology_closed);
    mongoc_apm_set_server_heartbeat_started_cb(context->callbacks, apm_server_heartbeat_started);
    mongoc_apm_set_server_heartbeat_succeeded_cb(context->callbacks, apm_server_heartbeat_succeeded);
    mongoc_apm_set_server_heartbeat_failed_cb(context->callbacks, apm_server_heartbeat_failed);

    mongoc_client_pool_set_apm_callbacks(pool, context->callbacks, (void *)context);
    return context;
}

void ast_mongo_apm_stop(void* context)
{
    apm_context_t* _context = (apm_context_t*)context;

    if (!context) {
        ast_log(LOG_ERROR, "no context specified.\n");
        return;
    }

    mongoc_apm_callbacks_destroy(_context->callbacks);
    ast_free(context);
}

static int config(int reload)
{
    int res = 0;
    struct ast_config *cfg = NULL;
    ast_log(LOG_DEBUG, "reload=%d\n", reload);
    do {
        const char *tmp;
        struct ast_variable *var;
        struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };

        cfg = ast_config_load(CONFIG_FILE, config_flags);
        if (!cfg || cfg == CONFIG_STATUS_FILEINVALID) {
            ast_log(LOG_WARNING, "unable to load config %s\n", CONFIG_FILE);
            break;
        }
        else if (cfg == CONFIG_STATUS_FILEUNCHANGED)
            break;

        if (!(var = ast_variable_browse(cfg, CATEGORY)))
            break;

        if ((tmp = ast_variable_retrieve(cfg, CATEGORY, "mongoc_log_level"))
        && (sscanf(tmp, "%u", &mongoc_log_level) != 1)) {
           ast_log(LOG_WARNING, "mongoc_log_level must be a 0..6, not '%s'\n", tmp);
           mongoc_log_level = MONGOC_LOG_LEVEL_ERROR;
        }
        if ((tmp = ast_variable_retrieve(cfg, CATEGORY, "apm_command_monitoring"))
        && (sscanf(tmp, "%u", &apm_command_monitoring) != 1)) {
           ast_log(LOG_WARNING, "apm_command_monitoring must be a 0|1, not '%s'\n", tmp);
           apm_command_monitoring = 0;
        }
        if ((tmp = ast_variable_retrieve(cfg, CATEGORY, "apm_sdam_monitoring"))
        && (sscanf(tmp, "%u", &apm_sdam_monitoring) != 1)) {
           ast_log(LOG_WARNING, "apm_sdam_monitoring must be a 0|1, not '%s'\n", tmp);
           apm_sdam_monitoring = 0;
        }
    } while (0);

    if (cfg && cfg != CONFIG_STATUS_FILEUNCHANGED && cfg != CONFIG_STATUS_FILEINVALID) {
        ast_config_destroy(cfg);
    }
    return res;
}

static int reload(void)
{
    int res;
    ast_log(LOG_DEBUG, "reloding...\n");
    mongoc_log_set_handler(NULL, NULL);
    res = config(1);
    mongoc_log_set_handler(mongoc_log_handler, NULL);
    return res;
}

static int unload_module(void)
{
    ast_log(LOG_DEBUG, "unloading...\n");
    mongoc_log_set_handler(NULL, NULL);
    mongoc_cleanup();
    return 0;
}

/*!
 * \brief Load the module
 *
 * Module loading including tests for configuration or dependencies.
 * This function can return AST_MODULE_LOAD_FAILURE, AST_MODULE_LOAD_DECLINE,
 * or AST_MODULE_LOAD_SUCCESS. If a dependency or environment variable fails
 * tests return AST_MODULE_LOAD_FAILURE. If the module can not load the
 * configuration file or other non-critical problem return
 * AST_MODULE_LOAD_DECLINE. On success return AST_MODULE_LOAD_SUCCESS.
 */
static int load_module(void)
{
    ast_log(LOG_DEBUG, "loading...\n");
    if (config(0))
        return AST_MODULE_LOAD_DECLINE;
    mongoc_init();
    mongoc_log_set_handler(mongoc_log_handler, NULL);
    return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_GLOBAL_SYMBOLS | AST_MODFLAG_LOAD_ORDER, "MongoDB resource",
    .support_level = AST_MODULE_SUPPORT_EXTENDED,
    .load = load_module,
    .unload = unload_module,
    .reload = reload,
    .load_pri = AST_MODPRI_REALTIME_DEPEND,
);
