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

/*! \li \ref res_mongodb.c uses the configuration file \ref res_mongodb.conf
 * \addtogroup configuration_file Configuration Files
 */

/*! 
 * \page res_mongodb.conf res_mongodb.conf
 * \verbinclude res_mongodb.conf.sample
 */

/*** MODULEINFO
    <depend>mongoc</depend>
    <depend>bson</depend>
    <support_level>extended</support_level>
 ***/


#include "asterisk.h"

ASTERISK_REGISTER_FILE()

#include "asterisk/module.h"
#include "asterisk/res_mongodb.h"

/*** DOCUMENTATION
    <function name="MongoDB" language="en_US">
        <synopsis>
            MongoDB Common Resources
        </synopsis>
        <syntax>
            <parameter name="property" required="true">
                <enumlist>
                    <enum name="transaction">
                        <para>Gets or sets the active transaction ID.  If set, and the transaction ID does not
                        exist and a <replaceable>database name</replaceable> is specified as an argument, it will be created.</para>
                    </enum>
                </enumlist>
            </parameter>
            <parameter name="argument" required="false" />
        </syntax>
        <description>
            <para>The MongoDB() function allows setting several properties to influence how a connected
            database processes transactions.</para>
        </description>
    </function>
 ***/

static int reload(void)
{
    ast_log(LOG_NOTICE, "reloding...\n");
    return 0;
}

static int unload_module(void)
{
    ast_log(LOG_WARNING, "unexpected unloading...\n");
    /* Prohibit unloading */
    mongoc_cleanup();
    return -1;
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
    ast_log(LOG_NOTICE, "loading...\n");
    mongoc_init();
    return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_GLOBAL_SYMBOLS | AST_MODFLAG_LOAD_ORDER, "MongoDB resource",
    .support_level = AST_MODULE_SUPPORT_EXTENDED,
    .load = load_module,
    .unload = unload_module,
    .reload = reload,
    .load_pri = AST_MODPRI_REALTIME_DEPEND,
);
