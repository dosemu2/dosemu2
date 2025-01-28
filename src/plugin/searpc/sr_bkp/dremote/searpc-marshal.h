
static char *
marshal_int__int64_int64_int_int_int64 (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    gint64 param1 = json_array_get_int_element (param_array, 1);
    gint64 param2 = json_array_get_int_element (param_array, 2);
    int param3 = json_array_get_int_element (param_array, 3);
    int param4 = json_array_get_int_element (param_array, 4);
    gint64 param5 = json_array_get_int_element (param_array, 5);

    int ret = ((int (*)(gint64, gint64, int, int, gint64, GError **))func) (param1, param2, param3, param4, param5, &error);

    json_t *object = json_object ();
    searpc_set_int_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}


static char *
marshal_int__int64_int64_int (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    gint64 param1 = json_array_get_int_element (param_array, 1);
    gint64 param2 = json_array_get_int_element (param_array, 2);
    int param3 = json_array_get_int_element (param_array, 3);

    int ret = ((int (*)(gint64, gint64, int, GError **))func) (param1, param2, param3, &error);

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
marshal_int__int (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    int param1 = json_array_get_int_element (param_array, 1);

    int ret = ((int (*)(int, GError **))func) (param1, &error);

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
marshal_int__int_int64 (void *func, json_t *param_array, gsize *ret_len)
{
    GError *error = NULL;
    int param1 = json_array_get_int_element (param_array, 1);
    gint64 param2 = json_array_get_int_element (param_array, 2);

    int ret = ((int (*)(int, gint64, GError **))func) (param1, param2, &error);

    json_t *object = json_object ();
    searpc_set_int_to_ret_object (object, ret);
    return searpc_marshal_set_ret_common (object, ret_len, error);
}

static void register_marshals(void)
{

    {
        searpc_server_register_marshal (searpc_signature_int__int64_int64_int_int_int64(), marshal_int__int64_int64_int_int_int64);
    }


    {
        searpc_server_register_marshal (searpc_signature_int__int64_int64_int(), marshal_int__int64_int64_int);
    }


    {
        searpc_server_register_marshal (searpc_signature_int__void(), marshal_int__void);
    }


    {
        searpc_server_register_marshal (searpc_signature_int__int(), marshal_int__int);
    }


    {
        searpc_server_register_marshal (searpc_signature_int__int_int(), marshal_int__int_int);
    }


    {
        searpc_server_register_marshal (searpc_signature_int__int_int64(), marshal_int__int_int64);
    }

}
