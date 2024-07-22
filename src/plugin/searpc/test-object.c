#include "test-object.h"

G_DEFINE_TYPE (TestObject, test_object, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_RET,
    PROP_ERRN,
    PROP_ARGSERR,
    N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void test_object_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    TestObject *self = TEST_OBJECT (object);
    switch (property_id) {
        case PROP_RET:
            self->ret = g_value_get_int (value);
            break;
        case PROP_ERRN:
            self->errn = g_value_get_int (value);
            break;
        case PROP_ARGSERR:
            self->args_err = g_value_get_int (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void test_object_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    TestObject *self = TEST_OBJECT (object);
    switch (property_id) {
        case PROP_RET:
            g_value_set_int (value, self->ret);
            break;
        case PROP_ERRN:
            g_value_set_int (value, self->errn);
            break;
        case PROP_ARGSERR:
            g_value_set_int (value, self->args_err);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void test_object_dispose (GObject *object)
{
    G_OBJECT_CLASS (test_object_parent_class)->dispose(object);
}

static void test_object_finalize (GObject *object)
{
    G_OBJECT_CLASS (test_object_parent_class)->finalize(object);
}

static void test_object_class_init(TestObjectClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = test_object_set_property;
    gobject_class->get_property = test_object_get_property;
    gobject_class->dispose = test_object_dispose;
    gobject_class->finalize = test_object_finalize;

    obj_properties[PROP_RET] = g_param_spec_int ("len", "", "", -256, 256, 0, G_PARAM_READWRITE);
    obj_properties[PROP_ERRN] = g_param_spec_int ("errn", "", "", 0, 256, 0, G_PARAM_READWRITE);
    obj_properties[PROP_ARGSERR] = g_param_spec_int ("argserr", "", "", 0, 256, 0, G_PARAM_READWRITE);
    g_object_class_install_properties (gobject_class, N_PROPERTIES, obj_properties);
}

static void test_object_init(TestObject *self)
{
    self->ret = 0;
    self->errn = 0;
    self->args_err = 0;
}
