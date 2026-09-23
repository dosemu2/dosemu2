
inline static gchar *
searpc_signature_int__string()
{
    return searpc_compute_signature ("int", 1, "string");
}


inline static gchar *
searpc_signature_int__void()
{
    return searpc_compute_signature ("int", 0);
}


inline static gchar *
searpc_signature_object__int_string_int()
{
    return searpc_compute_signature ("object", 3, "int", "string", "int");
}


inline static gchar *
searpc_signature_object__string_int_int()
{
    return searpc_compute_signature ("object", 3, "string", "int", "int");
}


inline static gchar *
searpc_signature_object__int_string_int_int()
{
    return searpc_compute_signature ("object", 4, "int", "string", "int", "int");
}


inline static gchar *
searpc_signature_object__int_string_int64_int64()
{
    return searpc_compute_signature ("object", 4, "int", "string", "int64", "int64");
}


inline static gchar *
searpc_signature_object__int_string()
{
    return searpc_compute_signature ("object", 2, "int", "string");
}


inline static gchar *
searpc_signature_object__int_string_int_string()
{
    return searpc_compute_signature ("object", 4, "int", "string", "int", "string");
}


inline static gchar *
searpc_signature_int__int_string()
{
    return searpc_compute_signature ("int", 2, "int", "string");
}


inline static gchar *
searpc_signature_int__string_int_int()
{
    return searpc_compute_signature ("int", 3, "string", "int", "int");
}


inline static gchar *
searpc_signature_object__int_int()
{
    return searpc_compute_signature ("object", 2, "int", "int");
}


inline static gchar *
searpc_signature_object__int_int_string_int()
{
    return searpc_compute_signature ("object", 4, "int", "int", "string", "int");
}


inline static gchar *
searpc_signature_int__int_int_string()
{
    return searpc_compute_signature ("int", 3, "int", "int", "string");
}


inline static gchar *
searpc_signature_int__int()
{
    return searpc_compute_signature ("int", 1, "int");
}

