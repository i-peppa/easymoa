#include <ext/standard/php_smart_string.h>
#include "php_network.h"
#include <netinet/tcp.h>  /* TCP_NODELAY */
#include <zend_exceptions.h>
#include "common.h"

extern zend_class_entry *easymoa_ce;
extern zend_class_entry *easymoa_exception_ce;

PHP_EASYMOA_API EasymoaSock*
easymoa_sock_create(char *host, int host_len, unsigned short port, double timeout,
                  int persistent, char *persistent_id,
                  zend_bool lazy_connect)
{
    EasymoaSock *easymoa_sock;

    easymoa_sock         = ecalloc(1, sizeof(EasymoaSock));
    easymoa_sock->host   = estrndup(host, host_len);
    easymoa_sock->stream = NULL;
    easymoa_sock->status = EASYMOA_SOCK_STATUS_DISCONNECTED;
    easymoa_sock->persistent = persistent;
    easymoa_sock->lazy_connect = lazy_connect;

    if(persistent_id) {
        size_t persistent_id_len = strlen(persistent_id);
        easymoa_sock->persistent_id = ecalloc(persistent_id_len + 1, 1);
        memcpy(easymoa_sock->persistent_id, persistent_id, persistent_id_len);
    } else {
        easymoa_sock->persistent_id = NULL;
    }

    easymoa_sock->host[host_len] = '\0';

    easymoa_sock->port    = port;
    easymoa_sock->timeout = timeout;
    easymoa_sock->read_timeout = timeout;

    easymoa_sock->err = NULL;
    easymoa_sock->err_len = 0;
    easymoa_sock->readonly = 0;
    easymoa_sock->issend = 0;

    return easymoa_sock;
}


/**
 * easymoa_sock_connect
 */
PHP_EASYMOA_API int easymoa_sock_connect(EasymoaSock *easymoa_sock TSRMLS_DC)
{
    struct timeval tv, read_tv, *tv_ptr = NULL;
    char *host = NULL, *persistent_id = NULL;
    zend_string *errstr;
    const char *fmtstr = "%s:%d";
    int host_len, err = 0;
    php_netstream_data_t *sock;
    int tcp_flag = 1;

    if (easymoa_sock->stream != NULL) {
        easymoa_sock_disconnect(easymoa_sock TSRMLS_CC);
    }

    tv.tv_sec  = (time_t)easymoa_sock->timeout;
    tv.tv_usec = (int)((easymoa_sock->timeout - tv.tv_sec) * 1000000);
    if(tv.tv_sec != 0 || tv.tv_usec != 0) {
        tv_ptr = &tv;
    }

    read_tv.tv_sec  = (time_t)easymoa_sock->read_timeout;
    read_tv.tv_usec = (int)((easymoa_sock->read_timeout-read_tv.tv_sec)*1000000);

    if(easymoa_sock->host[0] == '/' && easymoa_sock->port < 1) {
        host_len = spprintf(&host, 0, "unix://%s", easymoa_sock->host);
    } else {
        if(easymoa_sock->port == 0)
            easymoa_sock->port = 6379;

#ifdef HAVE_IPV6
        /* If we've got IPv6 and find a colon in our address, convert to proper
         * IPv6 [host]:port format */
        if (strchr(easymoa_sock->host, ':') != NULL) {
            fmtstr = "[%s]:%d";
        }
#endif
        host_len = spprintf(&host, 0, fmtstr, easymoa_sock->host, easymoa_sock->port);
    }

    if (easymoa_sock->persistent) {
        if (easymoa_sock->persistent_id) {
            spprintf(&persistent_id, 0, "phpredis:%s:%s", host,
                easymoa_sock->persistent_id);
        } else {
            spprintf(&persistent_id, 0, "phpredis:%s:%f", host,
                easymoa_sock->timeout);
        }
    }

    easymoa_sock->stream = php_stream_xport_create(host, host_len,
        0, STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
        persistent_id, tv_ptr, NULL, &errstr, &err);

    if (persistent_id) {
        efree(persistent_id);
    }

    efree(host);

    if (!easymoa_sock->stream) {
        if (errstr) efree(errstr);
        return -1;
    }

    /* set TCP_NODELAY */
    sock = (php_netstream_data_t*)easymoa_sock->stream->abstract;
    setsockopt(sock->socket, IPPROTO_TCP, TCP_NODELAY, (char *) &tcp_flag,
        sizeof(int));

    php_stream_auto_cleanup(easymoa_sock->stream);

    if(tv.tv_sec != 0 || tv.tv_usec != 0) {
        php_stream_set_option(easymoa_sock->stream,PHP_STREAM_OPTION_READ_TIMEOUT,
            0, &read_tv);
    }
    php_stream_set_option(easymoa_sock->stream,
        PHP_STREAM_OPTION_WRITE_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);

    easymoa_sock->status = EASYMOA_SOCK_STATUS_CONNECTED;

    return 0;
}


/**
 * easymoa_sock_server_open
 */
PHP_EASYMOA_API int easymoa_sock_server_open(EasymoaSock *easymoa_sock, int force_connect TSRMLS_DC)
{
    int res = -1;

    switch (easymoa_sock->status) {
        case EASYMOA_SOCK_STATUS_DISCONNECTED:
            return easymoa_sock_connect(easymoa_sock TSRMLS_CC);
        case EASYMOA_SOCK_STATUS_CONNECTED:
            res = 0;
        break;
        case EASYMOA_SOCK_STATUS_UNKNOWN:
            if (force_connect > 0 && easymoa_sock_connect(easymoa_sock TSRMLS_CC) < 0) {
                res = -1;
            } else {
                res = 0;

                easymoa_sock->status = EASYMOA_SOCK_STATUS_CONNECTED;
            }
        break;
    }

    return res;
}



/**
 * easymoa_sock_read_bulk_reply
 */
PHP_EASYMOA_API char *easymoa_sock_read_bulk_reply(EasymoaSock *easymoa_sock, int bytes TSRMLS_DC)
{
    int offset = 0;
    size_t got;

    char *reply, c[2];

        if (-1 == bytes) {
        return NULL;
    }

        reply = emalloc(bytes+1);

        while(offset < bytes) {
            got = php_stream_read(easymoa_sock->stream, reply + offset, 
                bytes-offset);
            if (got <= 0) {
                /* Error or EOF */
                zend_throw_exception(easymoa_exception_ce, 
                    "socket error on read socket", 0 TSRMLS_CC);
                break;
            }
            offset += got;
        }
    php_stream_read(easymoa_sock->stream, c, 2);

    reply[bytes] = 0;
    return reply;
}


/**
 * easymoa_sock_read
 */
PHP_EASYMOA_API char *easymoa_sock_read(EasymoaSock *easymoa_sock, int *buf_len TSRMLS_DC)
{
    char inbuf[1024];
    size_t err_len;

    *buf_len = 0;
    // if(-1 == easymoa_check_eof(easymoa_sock, 0 TSRMLS_CC)) {
    //     return NULL;
    // }

    if(php_stream_gets(easymoa_sock->stream, inbuf, 1024) == NULL) {
        //EASYMOA_STREAM_CLOSE_MARK_FAILED(easymoa_sock);
        zend_throw_exception(easymoa_exception_ce, "read error on connection", 
                             0 TSRMLS_CC);
        return NULL;
    }

    switch(inbuf[0]) {
        case '-':
            err_len = strlen(inbuf+1) - 2;
            easymoa_sock_set_err(easymoa_sock, inbuf+1, err_len);

            /* Handle stale data error */
            if(memcmp(inbuf + 1, "-ERR SYNC ", 10) == 0) {
                zend_throw_exception(easymoa_exception_ce, 
                    "SYNC with master in progress", 0 TSRMLS_CC);
            }
            return NULL;
        case '$':
            *buf_len = atoi(inbuf + 1);
            return easymoa_sock_read_bulk_reply(easymoa_sock, *buf_len TSRMLS_CC);

        case '*':
            /* For null multi-bulk replies (like timeouts from brpoplpush): */
            if(memcmp(inbuf + 1, "-1", 2) == 0) {
                return NULL;
            }
            /* fall through */

        case '+':
        case ':':
        /* Single Line Reply */
            /* :123\r\n */
            *buf_len = strlen(inbuf) - 2;
            if(*buf_len >= 2) {
                return estrndup(inbuf, *buf_len);
            }
        default:
            zend_throw_exception_ex(
                easymoa_exception_ce,
                0 TSRMLS_CC,
                "protocol error, got '%c' as reply type byte\n",
                inbuf[0]
            );
    }

    return NULL;
}


/**
 * easymoa_sock_write
 */
PHP_EASYMOA_API int easymoa_sock_write(EasymoaSock *easymoa_sock, char *cmd, size_t sz
                            TSRMLS_DC)
{
    if(easymoa_sock && easymoa_sock->status == EASYMOA_SOCK_STATUS_DISCONNECTED) {
        zend_throw_exception(easymoa_exception_ce, "Connection closed",
            0 TSRMLS_CC);
        return -1;
    }
    // if(-1 == easymoa_check_eof(easymoa_sock, 0 TSRMLS_CC)) {
    //     return -1;
    // }
    return php_stream_write(easymoa_sock->stream, cmd, sz);
}

/**
 * easymoa_free_socket
 */
PHP_EASYMOA_API void easymoa_free_socket(EasymoaSock *easymoa_sock)
{
    if(easymoa_sock->err) {
        efree(easymoa_sock->err);
    }
    if(easymoa_sock->auth) {
        efree(easymoa_sock->auth);
    }
    if(easymoa_sock->persistent_id) {
        efree(easymoa_sock->persistent_id);
    }
    efree(easymoa_sock->host);
    efree(easymoa_sock);
}


/**
 * easymoa_sock_disconnect
 */
PHP_EASYMOA_API int easymoa_sock_disconnect(EasymoaSock *easymoa_sock TSRMLS_DC)
{
    if (easymoa_sock == NULL) {
        return 1;
    }

    if (easymoa_sock->stream != NULL) {
            easymoa_sock->status = EASYMOA_SOCK_STATUS_DISCONNECTED;


            /* Stil valid? */
            if(easymoa_sock->stream && !easymoa_sock->persistent) {
                php_stream_close(easymoa_sock->stream);
            }
            easymoa_sock->stream = NULL;

            return 1;
    }

    return 0;
}


PHP_EASYMOA_API void easymoa_string_response(EasymoaSock *easymoa_sock, zval *z_tab, char **response, void *ctx) {

    int response_len;

    if ((*response = easymoa_sock_read(easymoa_sock, &response_len TSRMLS_CC))  == NULL) {
        return 0;
    }

}


int
easymoa_cmd_format_static(char **ret, char *keyword, char *format, char *key, int key_len)
{

    va_list ap;
    smart_string buf = {0};
    int l = strlen(keyword);



    /* add header */
    smart_string_appendc(&buf, '*');
    smart_string_append_long(&buf, strlen(format) + 1);
    smart_string_appendl(&buf, _NL, sizeof(_NL) - 1);
    smart_string_appendc(&buf, '$');
    smart_string_append_long(&buf, l);
    smart_string_appendl(&buf, _NL, sizeof(_NL) - 1);
    smart_string_appendl(&buf, keyword, l);
    smart_string_appendl(&buf, _NL, sizeof(_NL) - 1);
    smart_string_appendc(&buf, '$');
    smart_string_append_long(&buf, key_len);
    smart_string_appendl(&buf, _NL, sizeof(_NL) - 1);
    smart_string_appendl(&buf, key, key_len);
    smart_string_appendl(&buf, _NL, sizeof(_NL) - 1);
    smart_string_0(&buf);

    *ret = buf.c;

    return buf.len;

}


/* Generic command where we take a single key */
int easymoa_key_cmd(char *kw, char **cmd, int *cmd_len, char *key, int key_len)
{

    // Construct our command
    *cmd_len = easymoa_cmd_format_static(cmd, kw, "s", key, key_len);
    return SUCCESS;
}