
static char *
marshal_int__string (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    const char* param1 = json_array_get_string_or_null_element (param_array, 1);

    int ret = ((int (*)(const char*, GError **))func) (param1, &error);

    json_t *object = json_object ();
    searpc_set_int_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_int__void (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;

    int ret = ((int (*)(GError **))func) (&error);

    json_t *object = json_object ();
    searpc_set_int_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_object__int_string_int (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    int param1 = json_array_get_int_element (param_array, 1);
    const char* param2 = json_array_get_string_or_null_element (param_array, 2);
    int param3 = json_array_get_int_element (param_array, 3);

    GObject* ret = ((GObject* (*)(int, const char*, int, GError **))func) (param1, param2, param3, &error);

    json_t *object = json_object ();
    searpc_set_object_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_object__string_int_int (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    const char* param1 = json_array_get_string_or_null_element (param_array, 1);
    int param2 = json_array_get_int_element (param_array, 2);
    int param3 = json_array_get_int_element (param_array, 3);

    GObject* ret = ((GObject* (*)(const char*, int, int, GError **))func) (param1, param2, param3, &error);

    json_t *object = json_object ();
    searpc_set_object_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_object__int_string_int_int (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    int param1 = json_array_get_int_element (param_array, 1);
    const char* param2 = json_array_get_string_or_null_element (param_array, 2);
    int param3 = json_array_get_int_element (param_array, 3);
    int param4 = json_array_get_int_element (param_array, 4);

    GObject* ret = ((GObject* (*)(int, const char*, int, int, GError **))func) (param1, param2, param3, param4, &error);

    json_t *object = json_object ();
    searpc_set_object_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_object__int_string_int64_int64 (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    int param1 = json_array_get_int_element (param_array, 1);
    const char* param2 = json_array_get_string_or_null_element (param_array, 2);
    gint64 param3 = json_array_get_int_element (param_array, 3);
    gint64 param4 = json_array_get_int_element (param_array, 4);

    GObject* ret = ((GObject* (*)(int, const char*, gint64, gint64, GError **))func) (param1, param2, param3, param4, &error);

    json_t *object = json_object ();
    searpc_set_object_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_object__int_string (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    int param1 = json_array_get_int_element (param_array, 1);
    const char* param2 = json_array_get_string_or_null_element (param_array, 2);

    GObject* ret = ((GObject* (*)(int, const char*, GError **))func) (param1, param2, &error);

    json_t *object = json_object ();
    searpc_set_object_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_object__int_string_int_string (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    int param1 = json_array_get_int_element (param_array, 1);
    const char* param2 = json_array_get_string_or_null_element (param_array, 2);
    int param3 = json_array_get_int_element (param_array, 3);
    const char* param4 = json_array_get_string_or_null_element (param_array, 4);

    GObject* ret = ((GObject* (*)(int, const char*, int, const char*, GError **))func) (param1, param2, param3, param4, &error);

    json_t *object = json_object ();
    searpc_set_object_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_int__int_string (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    int param1 = json_array_get_int_element (param_array, 1);
    const char* param2 = json_array_get_string_or_null_element (param_array, 2);

    int ret = ((int (*)(int, const char*, GError **))func) (param1, param2, &error);

    json_t *object = json_object ();
    searpc_set_int_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_int__string_int_int (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    const char* param1 = json_array_get_string_or_null_element (param_array, 1);
    int param2 = json_array_get_int_element (param_array, 2);
    int param3 = json_array_get_int_element (param_array, 3);

    int ret = ((int (*)(const char*, int, int, GError **))func) (param1, param2, param3, &error);

    json_t *object = json_object ();
    searpc_set_int_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_int__int_int (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    int param1 = json_array_get_int_element (param_array, 1);
    int param2 = json_array_get_int_element (param_array, 2);

    int ret = ((int (*)(int, int, GError **))func) (param1, param2, &error);

    json_t *object = json_object ();
    searpc_set_int_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_int__int_int_string (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    int param1 = json_array_get_int_element (param_array, 1);
    int param2 = json_array_get_int_element (param_array, 2);
    const char* param3 = json_array_get_string_or_null_element (param_array, 3);

    int ret = ((int (*)(int, int, const char*, GError **))func) (param1, param2, param3, &error);

    json_t *object = json_object ();
    searpc_set_int_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}

static void register_marshals(void)
{

    {
        searpc_server_register_marshal (searpc_signature_int__string(), marshal_int__string);
    }


    {
        searpc_server_register_marshal (searpc_signature_int__void(), marshal_int__void);
    }


    {
        searpc_server_register_marshal (searpc_signature_object__int_string_int(), marshal_object__int_string_int);
    }


    {
        searpc_server_register_marshal (searpc_signature_object__string_int_int(), marshal_object__string_int_int);
    }


    {
        searpc_server_register_marshal (searpc_signature_object__int_string_int_int(), marshal_object__int_string_int_int);
    }


    {
        searpc_server_register_marshal (searpc_signature_object__int_string_int64_int64(), marshal_object__int_string_int64_int64);
    }


    {
        searpc_server_register_marshal (searpc_signature_object__int_string(), marshal_object__int_string);
    }


    {
        searpc_server_register_marshal (searpc_signature_object__int_string_int_string(), marshal_object__int_string_int_string);
    }


    {
        searpc_server_register_marshal (searpc_signature_int__int_string(), marshal_int__int_string);
    }


    {
        searpc_server_register_marshal (searpc_signature_int__string_int_int(), marshal_int__string_int_int);
    }


    {
        searpc_server_register_marshal (searpc_signature_int__int_int(), marshal_int__int_int);
    }


    {
        searpc_server_register_marshal (searpc_signature_int__int_int_string(), marshal_int__int_int_string);
    }

}
