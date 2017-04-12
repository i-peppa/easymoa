/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2017 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

//ONLY NTS MODE!!!!! ONLY PHP7

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_easymoa.h"
#include "common.h"


#define easymoa_sock_name "Easymoa Socket Buffer"

/* If you declare any globals in php_easymoa.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(easymoa)
*/

/* True global resources - no need for thread safety here */
static int le_easymoa;

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("easymoa.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_easymoa_globals, easymoa_globals)
    STD_PHP_INI_ENTRY("easymoa.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_easymoa_globals, easymoa_globals)
PHP_INI_END()
*/
/* }}} */

/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* The previous line is meant for vim and emacs, so it can correctly fold and
   unfold functions in source code. See the corresponding marks just before
   function definition, where the functions purpose is also documented. Please
   follow this convention for the convenience of others editing your code.
*/


/* {{{ php_easymoa_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_easymoa_init_globals(zend_easymoa_globals *easymoa_globals)
{
	easymoa_globals->global_value = 0;
	easymoa_globals->global_string = NULL;
}
/* }}} */



int le_easymoa_sock;
zend_class_entry *easymoa_ce;
zend_class_entry *easymoa_exception_ce;


/**
 * easymoa_destructor_easymoa_sock
 */
static void 
easymoa_destructor_easymoa_sock(zend_resource * rsrc TSRMLS_DC)
{
    EasymoaSock *easymoa_sock = (EasymoaSock *) rsrc->ptr;
    easymoa_sock_disconnect(easymoa_sock TSRMLS_CC);
    easymoa_free_socket(easymoa_sock);
}

static zend_always_inline int
easymoa_sock_get_instance(zval *id, EasymoaSock **easymoa_sock TSRMLS_DC, int no_throw)
{
    zval *socket;
    int resource_type = 0;

    if (Z_TYPE_P(id) == IS_OBJECT &&
        (socket = zend_hash_str_find(Z_OBJPROP_P(id), "socket", sizeof("socket") - 1)) != NULL
    ) {
#if (PHP_MAJOR_VERSION < 7)
        *easymoa_sock = (EasymoaSock *)zend_list_find(Z_LVAL_P(socket), &resource_type);
#else
        *easymoa_sock = NULL;

        if (Z_RES_P(socket) != NULL) {
            *easymoa_sock = (EasymoaSock *)Z_RES_P(socket)->ptr;
            resource_type = Z_RES_P(socket)->type;
        }
#endif
        if (*easymoa_sock && resource_type == le_easymoa_sock) {
            return 0;
        }
    }
    // Throw an exception unless we've been requested not to
    if (!no_throw) {
        zend_throw_exception(easymoa_exception_ce, "Redis server went away", 0 TSRMLS_CC);
    }
    return -1;
}


/**
 * easymoa_sock_get
 */
PHP_EASYMOA_API int
easymoa_sock_get(zval *id, EasymoaSock **easymoa_sock TSRMLS_DC, int no_throw)
{
    if (easymoa_sock_get_instance(id, easymoa_sock TSRMLS_CC, no_throw) < 0) {
        return -1;
    }

    if ((*easymoa_sock)->lazy_connect)
    {
        (*easymoa_sock)->lazy_connect = 0;
        if (easymoa_sock_server_open(*easymoa_sock, 1 TSRMLS_CC) < 0) {
            return -1;
        }
    }

    return 0;
}

PHP_EASYMOA_API int 
easymoa_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent) {
    zval *object, *socket;
    size_t host_len;
    char *host = NULL;

    zend_ulong port = 0;

    char *persistent_id = NULL;
    size_t persistent_id_len = -1;

    double timeout = 0.0;
    EasymoaSock *easymoa_sock  = NULL;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                     "Os|ldsl", &object, easymoa_ce, &host,
                                     &host_len, &port, &timeout, &persistent_id,
                                     &persistent_id_len)
                                     == FAILURE)
    {
        return FAILURE;
    }

    if (timeout < 0L || timeout > INT_MAX) {
        zend_throw_exception(easymoa_exception_ce, "Invalid timeout",
            0 TSRMLS_CC);
        return FAILURE;
    }

    /* If it's not a unix socket, set to default */
    if(port == 0 && host_len && host[0] != '/') {
        port = 6379;
    }

    easymoa_sock = easymoa_sock_create(host, host_len, port, timeout, persistent,
        persistent_id, 0);

    if (easymoa_sock_server_open(easymoa_sock, 1 TSRMLS_CC) < 0) {
        easymoa_free_socket(easymoa_sock);
        return FAILURE;
    }

    zval *id = zend_list_insert(easymoa_sock, le_easymoa_sock TSRMLS_CC);
    add_property_resource(object, "socket", Z_RES_P(id));

    return SUCCESS;
}

/* {{{ proto boolean Easymoa::pconnect(string host, int port [, double timeout])
 */
PHP_METHOD(Easymoa, pconnect)
{
    if (easymoa_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1) == FAILURE) {
        RETURN_FALSE;
    } else {
        RETURN_TRUE;
    }
}
/* }}} */


PHP_METHOD(Easymoa, send)
{
      EasymoaSock *easymoa_sock; 
      char *cmd; 
      char *key;
      int key_len;
      int cmd_len; 
      void *ctx=NULL;
      
      char *kw = "GET";

    if(easymoa_sock_get(getThis(), &easymoa_sock TSRMLS_CC, 0)<0) {
      RETURN_FALSE;
    } 

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) ==FAILURE) {
        RETURN_FALSE
    }

    if (easymoa_key_cmd(kw, &cmd, &cmd_len, key, key_len)==FAILURE) { 
            RETURN_FALSE; 
    }

    if(easymoa_sock_write(easymoa_sock, cmd, cmd_len TSRMLS_CC) < 0) { 
      efree(cmd);
      RETURN_FALSE;
    }

    easymoa_sock->issend = 1;

    efree(cmd);
    RETURN_TRUE;

    

}

PHP_METHOD(Easymoa, recv) 
{

    void *ctx=NULL;

    char *response;

    EasymoaSock *easymoa_sock; 

    if(easymoa_sock_get(getThis(), &easymoa_sock TSRMLS_CC, 0)<0) {
      RETURN_FALSE;
    } 

    if (easymoa_sock->issend <= 0) {
        RETURN_FALSE;
    }

    easymoa_string_response(easymoa_sock, NULL, &response, ctx);

    if (response == NULL) {
              RETURN_FALSE;
    } else {
              easymoa_sock->issend = 0;
              RETVAL_STRING(response);  
    }

    efree(response);
}



/* {{{ proto Redis Redis::__construct()
    Public constructor */
PHP_METHOD(Easymoa, __construct)
{
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
        RETURN_FALSE;
    }
}
/* }}} */

/* {{{ proto Redis Redis::__destruct()
    Public Destructor
 */
PHP_METHOD(Easymoa,__destruct) 
{
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
        RETURN_FALSE;
    }
}

/* {{{ easymoa_functions[]
 *
 * Every user visible function must have an entry in easymoa_functions[].
 */
static zend_function_entry easymoa_functions[] = {
	PHP_ME(Easymoa, __construct, NULL, ZEND_ACC_CTOR | ZEND_ACC_PUBLIC)
    PHP_ME(Easymoa, __destruct, NULL, ZEND_ACC_DTOR | ZEND_ACC_PUBLIC)
    PHP_ME(Easymoa, send, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Easymoa, recv, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Easymoa, pconnect, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(easymoa)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/

	zend_class_entry easymoa_class_entry;
  zend_class_entry easymoa_exception_class_entry;

  /* Easymoa class */
  INIT_CLASS_ENTRY(easymoa_class_entry, "Easymoa", easymoa_functions);
  easymoa_ce = zend_register_internal_class(&easymoa_class_entry TSRMLS_CC);
  

  le_easymoa_sock = zend_register_list_destructors_ex(
        easymoa_destructor_easymoa_sock,
        NULL,
        easymoa_sock_name, module_number
    );

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(easymoa)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(easymoa)
{
#if defined(COMPILE_DL_EASYMOA) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(easymoa)
{
	return SUCCESS;
}
/* }}} */


/**
 * PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(easymoa)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "Easymoa Support", "enabled");
    php_info_print_table_end();
}


/* {{{ easymoa_module_entry
 */
zend_module_entry easymoa_module_entry = {
	STANDARD_MODULE_HEADER,
	"easymoa",
	easymoa_functions,
	PHP_MINIT(easymoa),
	PHP_MSHUTDOWN(easymoa),
	PHP_RINIT(easymoa),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(easymoa),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(easymoa),
	PHP_EASYMOA_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_EASYMOA
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(easymoa)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
