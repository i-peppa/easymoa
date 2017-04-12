#include "php.h"
#include "php_ini.h"
#include "php_easymoa.h"

#define EASYMOA_SOCK_STATUS_FAILED       0
#define EASYMOA_SOCK_STATUS_DISCONNECTED 1
#define EASYMOA_SOCK_STATUS_UNKNOWN      2
#define EASYMOA_SOCK_STATUS_CONNECTED    3


#define _NL "\r\n"

/* {{{ struct EasymoaSock */
typedef struct {
    php_stream     *stream;
    char           *host;
    short          port;
    char           *auth;
    double         timeout;
    double         read_timeout;
    int            failed;
    int            status;
    int            persistent;
    char           *persistent_id;
    char           *err;
    int            err_len;
    zend_bool      lazy_connect;
    int            readonly;


    int            issend;
} EasymoaSock;
/* }}} */



PHP_EASYMOA_API char * easymoa_sock_read(EasymoaSock *easymoa_sock, int *buf_len TSRMLS_DC);
PHP_EASYMOA_API EasymoaSock* easymoa_sock_create(char *host, int host_len, unsigned short port, double timeout, int persistent, char *persistent_id, zend_bool lazy_connect);
PHP_EASYMOA_API int easymoa_sock_server_open(EasymoaSock *easymoa_sock, int force_connect TSRMLS_DC);
PHP_EASYMOA_API int easymoa_sock_disconnect(EasymoaSock *easymoa_sock TSRMLS_DC);
PHP_EASYMOA_API int easymoa_sock_write(EasymoaSock *easymoa_sock, char *cmd, size_t sz TSRMLS_DC);
PHP_EASYMOA_API void easymoa_string_response(EasymoaSock *easymoa_sock, zval *z_tab, char **response, void *ctx);



int easymoa_key_cmd(char *kw, char **cmd, int *cmd_len, char *key, int key_len);


int easymoa_cmd_format_static(char **ret, char *keyword, char *format, char *key, int key_len);