/*
 * MongoDB Common Resources
 *
 * Copyright: (c) 2015-2016 KINOSHITA minoru
 * License: GNU GENERAL PUBLIC LICENSE Version 2
 */

/*! \file
 * \author KINOSHITA minoru
 * \brief MongoDB resource manager
 */

#ifndef _ASTERISK_RES_MONGODB_H
#define _ASTERISK_RES_MONGODB_H

#include <libbson-1.0/bson.h>
#include <libmongoc-1.0/mongoc.h>

extern void* ast_mongo_apm_start(mongoc_client_pool_t* pool);
extern void ast_mongo_apm_stop(void* context);

#endif /* _ASTERISK_RES_MONGODB_H */
