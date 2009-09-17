#include <assert.h>
#include <stdio.h>
#include "empd.h"


void
empd_callback_set(empd_callback_t** cb, empd_callback_func_t f, void* data)
{
    empd_callback_t* new_cb = calloc(1, sizeof(empd_callback_t));
    assert(new_cb);
    assert(f);
    new_cb->func = f;
    new_cb->data = data;
    if(*cb)
        free(*cb);
    *cb = new_cb;
}

void
empd_callback_run(empd_callback_t* cb, void *value)
{
    if(cb)
        cb->func(value, cb->data);
    else
        printf("Attempt to run NULL callback\n");
}

void
empd_callback_once(empd_callback_t** cb, void *value)
{
    empd_callback_t* tmp = *cb;
    *cb=NULL;
    empd_callback_run(tmp, value);
    empd_callback_free(tmp);
}

void
empd_callback_free(empd_callback_t* cb)
{
    free(cb);
}

