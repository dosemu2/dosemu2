#ifndef TEST_OBJECT_H
#define TEST_OBJECT_H

#include <glib.h>
#include <glib-object.h>

GType test_object_get_type (void);

#define TEST_OBJECT_TYPE            (test_object_get_type())
#define TEST_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_OBJECT_TYPE, TestObject))
#define IS_TEST_OBJCET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_OBJCET_TYPE))
#define TEST_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_OBJECT_TYPE, TestObjectClass))
#define IS_TEST_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_OBJECT_TYPE))
#define TEST_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_OBJECT_TYPE, TestObjectClass))

typedef struct _TestObject      TestObject;
typedef struct _TestObjectClass TestObjectClass;

struct _TestObject {
    GObject parent;
    int ret;
    int errn;
    int args_err;
};

struct _TestObjectClass {
    GObjectClass parent_class;
};

#endif
