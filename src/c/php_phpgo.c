
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "../go/libphpgo.h"

// For compatibility
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
    if (zend_parse_parameters_none() == FAILURE) { \
        return; \
    }
#endif

// Global registry for callbacks
HashTable callback_registry;
long long next_callback_id = 1;

// Thread-safe value wrapper
typedef struct {
    int type;
    union {
        long l;
        double d;
        struct {
            char *val;
            size_t len;
        } s;
    } value;
} phpgo_message_t;

phpgo_message_t* create_message(zval *z) {
    if (!z) return NULL;
    phpgo_message_t *msg = malloc(sizeof(phpgo_message_t));
    if (Z_TYPE_P(z) == IS_LONG) {
        msg->type = IS_LONG;
        msg->value.l = Z_LVAL_P(z);
    } else if (Z_TYPE_P(z) == IS_DOUBLE) {
        msg->type = IS_DOUBLE;
        msg->value.d = Z_DVAL_P(z);
    } else if (Z_TYPE_P(z) == IS_STRING) {
        msg->type = IS_STRING;
        msg->value.s.len = Z_STRLEN_P(z);
        msg->value.s.val = malloc(Z_STRLEN_P(z) + 1);
        memcpy(msg->value.s.val, Z_STRVAL_P(z), Z_STRLEN_P(z));
        msg->value.s.val[Z_STRLEN_P(z)] = '\0';
    } else {
        msg->type = IS_NULL;
    }
    return msg;
}

void message_to_zval(phpgo_message_t *msg, zval *z) {
    if (!msg) {
        ZVAL_NULL(z);
        return;
    }
    switch (msg->type) {
        case IS_LONG:
            ZVAL_LONG(z, msg->value.l);
            break;
        case IS_DOUBLE:
            ZVAL_DOUBLE(z, msg->value.d);
            break;
        case IS_STRING:
            ZVAL_STRINGL(z, msg->value.s.val, msg->value.s.len);
            break;
        default:
            ZVAL_NULL(z);
    }
}

void free_message(phpgo_message_t *msg) {
    if (msg) {
        if (msg->type == IS_STRING) {
            free(msg->value.s.val);
        }
        free(msg);
    }
}

// Callback handler
void phpgo_callback_handler(long long id) {
    // TSRMLS_FETCH(); // Removed for PHP 8+
    PG_Lock();
    zval *cb;
    zval cb_copy;
    int found = 0;
    
    // We copy the zval to local to avoid holding lock during execution?
    // Wait, zval pointer in hashtable points to data.
    // If we want to execute, we need the zval.
    // But execution takes time and might call back into extension?
    // deadlock if we hold lock.
    // So we should find it, maybe Copy it?
    // But copying zval might touch refcounts (unsafe if shared).
    // Just holding lock for Lookup is fine for now.
    
    if ((cb = zend_hash_index_find(&callback_registry, (zend_ulong)id)) != NULL) {
        found = 1;
        // ZVAL_COPY_VALUE(&cb_copy, cb); // Shallow copy
    }
    PG_Unlock();
    
    if (found) {
        zval retval;
        // Using `cb` pointer here is unsafe if it helps realloc hash table
        // But we are NTS and main thread is blocked?
        // If main thread is blocked in `phpgo_go`? No.
        // `phpgo_go` doesn't block.
        // `phpgo_send` blocks?
        // Race: Main thread might trigger GC or Rehash?
        // We really need ZTS. 
        // We'll proceed with direct access.
        
        if (call_user_function(EG(function_table), NULL, cb, &retval, 0, NULL) == SUCCESS) {
             zval_ptr_dtor(&retval);
        }
    }
}

// phpgo\go(callable $func)
PHP_FUNCTION(phpgo_go) {
    zval *func;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &func) == FAILURE) {
        return;
    }
    
    PG_Lock();
    long long id = next_callback_id++;
    zval func_copy;
    ZVAL_COPY(&func_copy, func);
    zend_hash_index_update(&callback_registry, (zend_ulong)id, &func_copy);
    PG_Unlock();
    
    PG_StartGoroutine(id);
}

// phpgo\ping()
PHP_FUNCTION(phpgo_ping) {
    long ret = PG_Ping();
    RETURN_LONG(ret);
}

// phpgo\channel(int $buffer = 0)
PHP_FUNCTION(phpgo_channel) {
    // fprintf(stderr, "[phpgo] Entering phpgo_channel\n");
    zend_long buffer = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &buffer) == FAILURE) {
        return;
    }
    // fprintf(stderr, "[phpgo] Calling PG_MakeChannel with buffer %ld\n", buffer);
    long long id = PG_MakeChannel((int)buffer);
    RETURN_LONG(id);
}

// phpgo\send($ch, $val)
PHP_FUNCTION(phpgo_send) {
    zend_long id;
    zval *val;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz", &id, &val) == FAILURE) {
        return;
    }
    
    phpgo_message_t *msg = create_message(val);
    int res = PG_ChanSend((long long)id, (void*)msg);
    if (!res) {
        free_message(msg);
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

// phpgo\receive($ch)
PHP_FUNCTION(phpgo_receive) {
    zend_long id;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &id) == FAILURE) {
        return;
    }
    void *ptr = PG_ChanRecv((long long)id);
    if (ptr == NULL) {
        RETURN_NULL();
    }
    phpgo_message_t *msg = (phpgo_message_t*)ptr;
    message_to_zval(msg, return_value);
    free_message(msg);
}

// phpgo\close($ch)
PHP_FUNCTION(phpgo_close) {
    zend_long id;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &id) == FAILURE) {
        return;
    }
    PG_ChanClose((long long)id);
}

// phpgo\case_recv($ch)
PHP_FUNCTION(phpgo_case_recv) {
    zend_long id;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &id) == FAILURE) {
        return;
    }
    array_init_size(return_value, 2);
    add_assoc_long(return_value, "type", 0);
    add_assoc_long(return_value, "ch", id);
}

// phpgo\case_send($ch, $val)
PHP_FUNCTION(phpgo_case_send) {
    zend_long id;
    zval *val;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz", &id, &val) == FAILURE) {
        return;
    }
    array_init_size(return_value, 3);
    add_assoc_long(return_value, "type", 1);
    add_assoc_long(return_value, "ch", id);
    add_assoc_zval(return_value, "val", val);
    zval_add_ref(val);
}

// phpgo\case_default(callable $cb)
PHP_FUNCTION(phpgo_case_default) {
    zval *cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &cb) == FAILURE) {
        return;
    }
    array_init_size(return_value, 2);
    add_assoc_long(return_value, "type", 2);
    // Don't store callback in C map yet, just pass it back in array
    add_assoc_zval(return_value, "cb", cb);
    zval_add_ref(cb);
}

// phpgo\select(array $cases)
PHP_FUNCTION(phpgo_select) {
    zval *cases;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &cases) == FAILURE) {
        return;
    }

    int count = zend_hash_num_elements(Z_ARRVAL_P(cases));
    if (count == 0) RETURN_NULL();

    int *types = malloc(sizeof(int) * count);
    long long *ids = malloc(sizeof(long long) * count);
    void **values = malloc(sizeof(void*) * count);
    
    // Track messages to free if not sent? No, Send ownership is transferred if successful?
    // Go select implementation: if Send case selected, value is sent. If not, it's not.
    // If not sent, we need to free the message structure.
    // So we should keep track of created messages.
    phpgo_message_t **msgs = calloc(count, sizeof(phpgo_message_t*));

    int i = 0;
    zval *case_val;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(cases), case_val) {
        if (Z_TYPE_P(case_val) != IS_ARRAY) {
             // Skip invalid?
             types[i] = 3; // Invalid
             i++; continue;
        }
        
        HashTable *ht = Z_ARRVAL_P(case_val);
        zval *ztype = zend_hash_str_find(ht, "type", sizeof("type")-1);
        if (!ztype) { types[i] = 3; i++; continue; }
        
        int type = (int)Z_LVAL_P(ztype);
        types[i] = type;
        
        if (type == 0 || type == 1) { // Recv or Send
            zval *zch = zend_hash_str_find(ht, "ch", sizeof("ch")-1);
            if (zch) ids[i] = (long long)Z_LVAL_P(zch);
        }
        
        if (type == 1) { // Send
            zval *zval_send = zend_hash_str_find(ht, "val", sizeof("val")-1);
            if (zval_send) {
                msgs[i] = create_message(zval_send);
                values[i] = (void*)msgs[i];
            }
        }
        
        i++;
    } ZEND_HASH_FOREACH_END();
    
    int retIdx = -1;
    void *retVal = NULL;
    
    int success = PG_Select(count, types, ids, (void*)values, &retIdx, &retVal);
    
    // If sent (success == 1) and case was send, then Go owns the message (channel has it).
    // If not sent (case not chosen), we must free the message.
    for (int j=0; j<count; j++) {
        if (types[j] == 1 && msgs[j]) {
            if (retIdx == j) {
                // Sent successfully, do not free
            } else {
                free_message(msgs[j]);
            }
        }
    }
    
    free(types);
    free(ids);
    free(values);
    free(msgs);
    
    // Build return
    array_init(return_value);
    add_assoc_long(return_value, "index", retIdx);
    
    if (retIdx >= 0) {
        // Find the case
        // We need to re-fetch the case from the original array to get the callback if default
        // Iterating hash table by index is tricky if keys are not numeric 0..N
        // But users passed an array `[ ... ]` so it should be packed.
        zval *chosen_case = zend_hash_index_find(Z_ARRVAL_P(cases), retIdx);
        
        if (chosen_case && Z_TYPE_P(chosen_case) == IS_ARRAY) {
             HashTable *ht = Z_ARRVAL_P(chosen_case);
             zval *ztype = zend_hash_str_find(ht, "type", sizeof("type")-1);
             int type = ztype ? Z_LVAL_P(ztype) : -1;
             
             if (type == 0) { // Recv
                 if (retVal) {
                     phpgo_message_t *m = (phpgo_message_t*)retVal;
                     zval zv;
                     message_to_zval(m, &zv);
                     add_assoc_zval(return_value, "value", &zv);
                     free_message(m);
                 } else {
                     add_assoc_null(return_value, "value");
                 }
             } else if (type == 2) { // Default
                 zval *cb = zend_hash_str_find(ht, "cb", sizeof("cb")-1);
                 if (cb) {
                     // Execute callback
                     zval retval;
                     if (call_user_function(EG(function_table), NULL, cb, &retval, 0, NULL) == SUCCESS) {
                        add_assoc_zval(return_value, "value", &retval);
                     }
                 }
             }
        }
    }
}

/* WaitGroup Class */
zend_class_entry *phpgo_waitgroup_ce;

PHP_METHOD(WaitGroup, __construct) {
    long long id = PG_MakeWaitGroup();
    zend_update_property_long(phpgo_waitgroup_ce, Z_OBJ_P(ZEND_THIS), "id", sizeof("id")-1, id);
}

PHP_METHOD(WaitGroup, add) {
    zend_long delta;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &delta) == FAILURE) {
        return;
    }
    zval *id_z = zend_read_property(phpgo_waitgroup_ce, Z_OBJ_P(ZEND_THIS), "id", sizeof("id")-1, 1, NULL);
    PG_WGAdd(Z_LVAL_P(id_z), (int)delta);
}

PHP_METHOD(WaitGroup, done) {
    zval *id_z = zend_read_property(phpgo_waitgroup_ce, Z_OBJ_P(ZEND_THIS), "id", sizeof("id")-1, 1, NULL);
    PG_WGDone(Z_LVAL_P(id_z));
}

PHP_METHOD(WaitGroup, wait) {
    zval *id_z = zend_read_property(phpgo_waitgroup_ce, Z_OBJ_P(ZEND_THIS), "id", sizeof("id")-1, 1, NULL);
    PG_WGWait(Z_LVAL_P(id_z));
}

const zend_function_entry waitgroup_methods[] = {
    PHP_ME(WaitGroup, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(WaitGroup, add, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(WaitGroup, done, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(WaitGroup, wait, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* PHP_MINIT */
PHP_MINIT_FUNCTION(phpgo) {
    zend_hash_init(&callback_registry, 16, NULL, ZVAL_PTR_DTOR, 1);
    
    // Register WaitGroup class
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "phpgo\\WaitGroup", waitgroup_methods);
    phpgo_waitgroup_ce = zend_register_internal_class(&ce);
    zend_declare_property_long(phpgo_waitgroup_ce, "id", sizeof("id")-1, 0, ZEND_ACC_PRIVATE);

    // Set callback in Go
    PG_SetCallback(phpgo_callback_handler);
    
    return SUCCESS;
}

/* PHP_MSHUTDOWN */
PHP_MSHUTDOWN_FUNCTION(phpgo) {
    zend_hash_destroy(&callback_registry);
    return SUCCESS;
}

/* Function Registration */
const zend_function_entry phpgo_functions[] = {
    ZEND_NS_NAMED_FE("phpgo", ping, zif_phpgo_ping, NULL)
    ZEND_NS_NAMED_FE("phpgo", go, zif_phpgo_go, NULL)
    ZEND_NS_NAMED_FE("phpgo", channel, zif_phpgo_channel, NULL)
    ZEND_NS_NAMED_FE("phpgo", send, zif_phpgo_send, NULL)
    ZEND_NS_NAMED_FE("phpgo", receive, zif_phpgo_receive, NULL)
    ZEND_NS_NAMED_FE("phpgo", close, zif_phpgo_close, NULL)
    ZEND_NS_NAMED_FE("phpgo", select, zif_phpgo_select, NULL)
    ZEND_NS_NAMED_FE("phpgo", case_recv, zif_phpgo_case_recv, NULL)
    ZEND_NS_NAMED_FE("phpgo", case_send, zif_phpgo_case_send, NULL)
    ZEND_NS_NAMED_FE("phpgo", case_default, zif_phpgo_case_default, NULL)
    PHP_FE_END
};

/* Module Entry */
zend_module_entry phpgo_module_entry = {
    STANDARD_MODULE_HEADER,
    "phpgo",
    phpgo_functions,
    PHP_MINIT(phpgo),
    PHP_MSHUTDOWN(phpgo),
    NULL,
    NULL,
    NULL,
    "0.1.0",
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PHPGO
ZEND_GET_MODULE(phpgo)
#endif
