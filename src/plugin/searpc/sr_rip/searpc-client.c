#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "searpc-client.h"
#include "searpc-utils.h"

static char*
searpc_client_fret__string (char *data, size_t len, GError **error);

static int
searpc_client_fret__int (char *data, size_t len, GError **error);

static gint64
searpc_client_fret__int64 (char *data, size_t len, GError **error);

static GObject*
searpc_client_fret__object (GType gtype, char *data,
                            size_t len, GError **error);

static GList*
searpc_client_fret__objlist (GType gtype, char *data,
                             size_t len, GError **error);

static json_t *
searpc_client_fret__json (char *data, size_t len, GError **error);


static void clean_objlist(GList *list)
{
    GList *ptr;
    for (ptr = list; ptr; ptr = ptr->next)
        g_object_unref(ptr->data);
    g_list_free (list);
}


SearpcClient *
searpc_client_new (void)
{
    return g_new0 (SearpcClient, 1);
}

void
searpc_client_free (SearpcClient *client)
{
    if (!client)
        return;

    g_free (client);
}

char *
searpc_client_transport_send (SearpcClient *client,
                              const gchar *fcall_str,
                              size_t fcall_len,
                              size_t *ret_len)
{
    return client->send(client->arg, fcall_str,
                        fcall_len, ret_len);
}

static char *
fcall_to_str (const char *fname, int n_params, va_list args, gsize *len)
{
    json_t *array;

    array = json_array ();
    json_array_append_new (array, json_string(fname));


    int i = 0;
    for (; i < n_params; i++) {
        const char *type = va_arg(args, const char *);
        void *value = va_arg(args, void *);
        if (strcmp(type, "int") == 0)
	    json_array_append_new (array, json_integer ((int)(long)value));
        else if (strcmp(type, "int64") == 0)
	    json_array_append_new (array, json_integer (*((gint64 *)value)));
        else if (strcmp(type, "string") == 0)
            json_array_add_string_or_null_element (array, (char *)value);
        else if (strcmp(type, "json") == 0)
            json_array_add_json_or_null_element (array, (const json_t *)value);
        else {
            g_warning ("unrecognized parameter type %s\n", type);
            return NULL;
        }
    }

    char *data = json_dumps (array,JSON_COMPACT);
    *len = strlen (data);
    json_decref(array);

    return data;
}

void
searpc_client_call (SearpcClient *client, const char *fname,
                    const char *ret_type, GType gobject_type,
                    void *ret_ptr, GError **error,
                    int n_params, ...)
{
    g_return_if_fail (fname != NULL);
    g_return_if_fail (ret_type != NULL);

    va_list args;
    gsize len, ret_len;
    char *fstr;

    va_start (args, n_params);
    fstr = fcall_to_str (fname, n_params, args, &len);
    va_end (args);
    if (!fstr) {
        g_set_error (error, DFT_DOMAIN, 0, "Invalid Parameter");
        return;
    }

    char *fret = searpc_client_transport_send (client, fstr, len, &ret_len);
    if (!fret) {
        g_free (fstr);
        g_set_error (error, DFT_DOMAIN, TRANSPORT_ERROR_CODE, TRANSPORT_ERROR);
        return;
    }

    if (strcmp(ret_type, "int") == 0)
        *((int *)ret_ptr) = searpc_client_fret__int (fret, ret_len, error);
    else if (strcmp(ret_type, "int64") == 0)
        *((gint64 *)ret_ptr) = searpc_client_fret__int64 (fret, ret_len, error);
    else if (strcmp(ret_type, "string") == 0)
        *((char **)ret_ptr) = searpc_client_fret__string (fret, len, error);
    else if (strcmp(ret_type, "object") == 0)
        *((GObject **)ret_ptr) = searpc_client_fret__object (gobject_type, fret,
                                                             ret_len, error);
    else if (strcmp(ret_type, "objlist") == 0)
        *((GList **)ret_ptr) = searpc_client_fret__objlist (gobject_type, fret,
                                                            ret_len, error);
    else if (strcmp(ret_type, "json") == 0)
        *((json_t **)ret_ptr) = searpc_client_fret__json(fret, ret_len, error);
    else
        g_warning ("unrecognized return type %s\n", ret_type);

    g_free (fstr);
    g_free (fret);
}

int
searpc_client_call__int (SearpcClient *client, const char *fname,
                         GError **error, int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, 0);

    va_list args;
    gsize len, ret_len;
    char *fstr;

    va_start (args, n_params);
    fstr = fcall_to_str (fname, n_params, args, &len);
    va_end (args);
    if (!fstr) {
        g_set_error (error, DFT_DOMAIN, 0, "Invalid Parameter");
        return 0;
    }

    char *fret = searpc_client_transport_send (client, fstr, len, &ret_len);
    if (!fret) {
        g_free (fstr);
        g_set_error (error, DFT_DOMAIN, TRANSPORT_ERROR_CODE, TRANSPORT_ERROR);
        return 0;
    }

    int ret = searpc_client_fret__int (fret, ret_len, error);
    g_free (fstr);
    g_free (fret);
    return ret;
}

gint64
searpc_client_call__int64 (SearpcClient *client, const char *fname,
                         GError **error, int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, 0);

    va_list args;
    gsize len, ret_len;
    char *fstr;

    va_start (args, n_params);
    fstr = fcall_to_str (fname, n_params, args, &len);
    va_end (args);
    if (!fstr) {
        g_set_error (error, DFT_DOMAIN, 0, "Invalid Parameter");
        return 0;
    }

    char *fret = searpc_client_transport_send (client, fstr, len, &ret_len);
    if (!fret) {
        g_free (fstr);
        g_set_error (error, DFT_DOMAIN, TRANSPORT_ERROR_CODE, TRANSPORT_ERROR);
        return 0;
    }

    gint64 ret = searpc_client_fret__int64 (fret, ret_len, error);
    g_free (fstr);
    g_free (fret);
    return ret;
}

char *
searpc_client_call__string (SearpcClient *client, const char *fname,
                            GError **error, int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, NULL);

    va_list args;
    gsize len, ret_len;
    char *fstr;

    va_start (args, n_params);
    fstr = fcall_to_str (fname, n_params, args, &len);
    va_end (args);
    if (!fstr) {
        g_set_error (error, DFT_DOMAIN, 0, "Invalid Parameter");
        return NULL;
    }

    char *fret = searpc_client_transport_send (client, fstr, len, &ret_len);
    if (!fret) {
        g_free (fstr);
        g_set_error (error, DFT_DOMAIN, TRANSPORT_ERROR_CODE, TRANSPORT_ERROR);
        return NULL;
    }

    char *ret = searpc_client_fret__string (fret, ret_len, error);
    g_free (fstr);
    g_free (fret);
    return ret;
}

GObject *
searpc_client_call__object (SearpcClient *client, const char *fname,
                            GType object_type,
                            GError **error, int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, NULL);
    g_return_val_if_fail (object_type != 0, NULL);

    va_list args;
    gsize len, ret_len;
    char *fstr;

    va_start (args, n_params);
    fstr = fcall_to_str (fname, n_params, args, &len);
    va_end (args);
    if (!fstr) {
        g_set_error (error, DFT_DOMAIN, 0, "Invalid Parameter");
        return NULL;
    }

    char *fret = searpc_client_transport_send (client, fstr, len, &ret_len);
    if (!fret) {
        g_free (fstr);
        g_set_error (error, DFT_DOMAIN, TRANSPORT_ERROR_CODE, TRANSPORT_ERROR);
        return NULL;
    }

    GObject *ret = searpc_client_fret__object (object_type, fret, ret_len, error);
    g_free (fstr);
    g_free (fret);
    return ret;
}

GList*
searpc_client_call__objlist (SearpcClient *client, const char *fname,
                             GType object_type,
                             GError **error, int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, NULL);
    g_return_val_if_fail (object_type != 0, NULL);

    va_list args;
    gsize len, ret_len;
    char *fstr;

    va_start (args, n_params);
    fstr = fcall_to_str (fname, n_params, args, &len);
    va_end (args);

    if (!fstr) {
        g_set_error (error, DFT_DOMAIN, 0, "Invalid Parameter");
        return NULL;
    }

    char *fret = searpc_client_transport_send (client, fstr, len, &ret_len);
    if (!fret) {
        g_free (fstr);
        g_set_error (error, DFT_DOMAIN, TRANSPORT_ERROR_CODE, TRANSPORT_ERROR);
        return NULL;
    }

    GList *ret = searpc_client_fret__objlist (object_type, fret, ret_len, error);
    g_free (fstr);
    g_free (fret);
    return ret;
}

json_t *
searpc_client_call__json (SearpcClient *client, const char *fname,
                          GError **error, int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, NULL);

    va_list args;
    gsize len, ret_len;
    char *fstr;

    va_start (args, n_params);
    fstr = fcall_to_str (fname, n_params, args, &len);
    va_end (args);
    if (!fstr) {
        g_set_error (error, DFT_DOMAIN, 0, "Invalid Parameter");
        return NULL;
    }

    char *fret = searpc_client_transport_send (client, fstr, len, &ret_len);
    if (!fret) {
        g_free (fstr);
        g_set_error (error, DFT_DOMAIN, TRANSPORT_ERROR_CODE, TRANSPORT_ERROR);
        return NULL;
    }

    json_t *ret = searpc_client_fret__json (fret, ret_len, error);
    g_free (fstr);
    g_free (fret);
    return ret;
}



typedef struct {
    SearpcClient *client;
    AsyncCallback callback;
    const gchar *ret_type;
    GType gtype;                /* to specify the specific gobject type
                                 if ret_type is object or objlist */
    void *cbdata;
} AsyncCallData;

int
searpc_client_generic_callback (char *retstr, size_t len,
                                void *vdata, const char *errstr)
{
    AsyncCallData *data = vdata;
    GError *error = NULL;
    void *result = NULL;
    int ret;
    gint64 ret64;

    if (errstr) {
        g_set_error (&error, DFT_DOMAIN,
                     500, "Transport error: %s", errstr);
        data->callback (NULL, data->cbdata, error);
        g_error_free (error);
    } else {
        /* parse result and call the callback */
        if (strcmp(data->ret_type, "int") == 0) {
            ret = searpc_client_fret__int (retstr, len, &error);
            result = (void *)&ret;
        } if (strcmp(data->ret_type, "int64") == 0) {
            ret64 = searpc_client_fret__int64 (retstr, len, &error);
            result = (void *)&ret64;
        } else if (strcmp(data->ret_type, "string") == 0) {
            result = (void *)searpc_client_fret__string (retstr, len, &error);
        } else if (strcmp(data->ret_type, "object") == 0) {
            result = (void *)searpc_client_fret__object (
                data->gtype, retstr, len, &error);
        } else if (strcmp(data->ret_type, "objlist") == 0) {
            result = (void *)searpc_client_fret__objlist (
                data->gtype, retstr, len, &error);
        } else if (strcmp(data->ret_type, "json") == 0) {
            result = (void *)searpc_client_fret__json (retstr, len, &error);
        }

        data->callback (result, data->cbdata, error);

        if (strcmp(data->ret_type, "string") == 0) {
            g_free ((char *)result);
        } else if (strcmp(data->ret_type, "object") == 0) {
            if (result) g_object_unref ((GObject*)result);
        } else if (strcmp(data->ret_type, "objlist") == 0) {
            clean_objlist ((GList *)result);
        } else if (strcmp(data->ret_type, "json") == 0) {
            json_decref ((json_t *)result);
        }
    }
    g_free (data);

    return 0;
}


static int
searpc_client_async_call_v (SearpcClient *client,
                            const char *fname,
                            AsyncCallback callback,
                            const gchar *ret_type,
                            GType gtype,
                            void *cbdata,
                            int n_params,
                            va_list args)
{
    gsize len;
    char *fstr;

    fstr = fcall_to_str (fname, n_params, args, &len);
    if (!fstr)
        return -1;

    int ret;
    AsyncCallData *data = g_new0(AsyncCallData, 1);
    data->client = client;
    data->callback = callback;
    data->ret_type = ret_type;
    data->gtype = gtype;
    data->cbdata = cbdata;

    ret = client->async_send (client->async_arg, fstr, len, data);

    g_free(fstr);
    if (ret < 0) {
        g_free (data);
        return -1;
    }
    return 0;
}

int
searpc_client_async_call__int (SearpcClient *client,
                               const char *fname,
                               AsyncCallback callback, void *cbdata,
                               int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, -1);

    va_list args;
    int ret;

    va_start (args, n_params);
    ret = searpc_client_async_call_v (client, fname, callback, "int", 0, cbdata,
                                      n_params, args);
    va_end (args);
    return ret;
}

int
searpc_client_async_call__int64 (SearpcClient *client,
                                 const char *fname,
                                 AsyncCallback callback, void *cbdata,
                                 int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, -1);

    va_list args;
    int ret;

    va_start (args, n_params);
    ret = searpc_client_async_call_v (client, fname, callback, "int64", 0, cbdata,
                                      n_params, args);
    va_end (args);
    return ret;
}

int
searpc_client_async_call__string (SearpcClient *client,
                                  const char *fname,
                                  AsyncCallback callback, void *cbdata,
                                  int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, -1);

    va_list args;
    int ret;

    va_start (args, n_params);
    ret = searpc_client_async_call_v (client, fname, callback, "string", 0, cbdata,
                                      n_params, args);
    va_end (args);
    return ret;
}

int
searpc_client_async_call__object (SearpcClient *client,
                                  const char *fname,
                                  AsyncCallback callback,
                                  GType object_type, void *cbdata,
                                  int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, -1);

    va_list args;
    int ret;

    va_start (args, n_params);
    ret = searpc_client_async_call_v (client, fname, callback, "object",
                                      object_type, cbdata,
                                      n_params, args);
    va_end (args);
    return ret;
}

int
searpc_client_async_call__objlist (SearpcClient *client,
                                   const char *fname,
                                   AsyncCallback callback,
                                   GType object_type, void *cbdata,
                                   int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, -1);

    va_list args;
    int ret;

    va_start (args, n_params);
    ret = searpc_client_async_call_v (client, fname, callback, "objlist",
                                      object_type, cbdata,
                                      n_params, args);
    va_end (args);
    return ret;
}

int
searpc_client_async_call__json (SearpcClient *client,
                                const char *fname,
                                AsyncCallback callback, void *cbdata,
                                int n_params, ...)
{
    g_return_val_if_fail (fname != NULL, -1);

    va_list args;
    int ret;

    va_start (args, n_params);
    ret = searpc_client_async_call_v (client, fname, callback, "json",
                                      0, cbdata,
                                      n_params, args);
    va_end (args);
    return ret;
}


/*
 * Returns -1 if error happens in parsing data or data contains error
 * message. In this case, the calling function should simply return
 * to up layer.
 *
 * Returns 0 otherwise, and root is set to the root node, object set
 * to the root node's containing object.
 */
static int
handle_ret_common (char *data, size_t len, json_t **object, GError **error)
{
    int err_code;
    const char *err_msg;
    json_error_t jerror;

    g_return_val_if_fail (object != 0, -1);

    *object=json_loadb(data,len,0,&jerror);
    if (*object == NULL) {
        setjetoge(&jerror,error);
        json_decref (*object);
        return -1;
    }

    if (json_object_get (*object, "err_code")) {
        err_code = json_integer_value(json_object_get (*object, "err_code"));
        err_msg = json_string_value(json_object_get (*object, "err_msg"));
        g_set_error (error, DFT_DOMAIN,
                     err_code, "%s", err_msg);
        json_decref (*object);
        return -1;
    }

    return 0;
}


char *
searpc_client_fret__string (char *data, size_t len, GError **error)
{
    json_t *object = NULL;
    gchar *ret_str = NULL;

    if (handle_ret_common(data, len, &object, error) == 0) {
        ret_str = g_strdup (
            json_object_get_string_or_null_member (object, "ret"));
        json_decref (object);
        return ret_str;
    }

    return NULL;
}

int
searpc_client_fret__int (char *data, size_t len, GError **error)
{
    json_t *object = NULL;
    int ret;

    if (handle_ret_common(data, len, &object, error) == 0) {
        ret = json_integer_value (json_object_get(object, "ret"));
        json_decref(object);
        return ret;
    }

    return -1;
}

gint64
searpc_client_fret__int64 (char *data, size_t len, GError **error)
{
    json_t *object = NULL;
    gint64 ret;

    if (handle_ret_common(data, len, &object, error) == 0) {
        ret = json_integer_value (json_object_get(object, "ret"));
        json_decref(object);
        return ret;
    }

    return -1;
}

GObject*
searpc_client_fret__object (GType gtype, char *data, size_t len, GError **error)
{
    json_t  *object = NULL;
    GObject *ret = NULL;
    json_t  *member;

    if (handle_ret_common(data, len, &object, error) == 0) {
        member = json_object_get (object, "ret");
        if (json_is_null(member)) {
            json_decref(object);
            return NULL;
        }

        ret = json_gobject_deserialize(gtype, member);
        json_decref(object);
        return ret;
    }

    return NULL;
}

GList*
searpc_client_fret__objlist (GType gtype, char *data, size_t len, GError **error)
{
    json_t *object = NULL;
    GList  *ret = NULL;

    if (handle_ret_common(data, len, &object, error) == 0) {
        const json_t *array = json_object_get (object, "ret");
        if (json_is_null(array)) {
            json_decref(object);
            return NULL;
        }

        g_assert (array);

        int i;
        for (i = 0; i < json_array_size(array); i++) {
            json_t *member = json_array_get (array, i);
            GObject *obj = json_gobject_deserialize(gtype, member);
            if (obj == NULL) {
                g_set_error (error, DFT_DOMAIN, 503,
                             "Invalid data: object list contains null");
                clean_objlist(ret);
                json_decref(object);
                return NULL;
            }
            ret = g_list_prepend (ret, obj);
        }
        json_decref(object);
        return g_list_reverse(ret);
    }
    return NULL;
}

json_t *
searpc_client_fret__json (char *data, size_t len, GError **error)
{
    json_t *object = NULL;

    if (handle_ret_common(data, len, &object, error) == 0) {
        const json_t *ret_obj = json_object_get (object, "ret");
        if (!ret_obj || json_is_null(ret_obj)) {
            json_decref(object);
            return NULL;
        }

        g_assert (ret_obj);
        json_t *ret = json_deep_copy(ret_obj);

        json_decref(object);
        return ret;
    }
    return NULL;
}
