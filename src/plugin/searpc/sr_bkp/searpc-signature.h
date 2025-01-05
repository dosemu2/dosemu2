
inline static gchar *
searpc_signature_int__string(void)
{
    return searpc_compute_signature ("int", 1, "string");
}


inline static gchar *
searpc_signature_int__void(void)
{
    return searpc_compute_signature ("int", 0);
}


inline static gchar *
searpc_signature_object__int_string_int(void)
{
    return searpc_compute_signature ("object", 3, "int", "string", "int");
}


inline static gchar *
searpc_signature_object__string_int_int(void)
{
    return searpc_compute_signature ("object", 3, "string", "int", "int");
}


inline static gchar *
searpc_signature_object__int_string_int_int(void)
{
    return searpc_compute_signature ("object", 4, "int", "string", "int", "int");
}


inline static gchar *
searpc_signature_object__int_string_int64_int64(void)
{
    return searpc_compute_signature ("object", 4, "int", "string", "int64", "int64");
}


inline static gchar *
searpc_signature_object__int_string(void)
{
    return searpc_compute_signature ("object", 2, "int", "string");
}


inline static gchar *
searpc_signature_object__int_string_int_string(void)
{
    return searpc_compute_signature ("object", 4, "int", "string", "int", "string");
}


inline static gchar *
searpc_signature_int__int_string(void)
{
    return searpc_compute_signature ("int", 2, "int", "string");
}


inline static gchar *
searpc_signature_int__string_int_int(void)
{
    return searpc_compute_signature ("int", 3, "string", "int", "int");
}


inline static gchar *
searpc_signature_int__int_int(void)
{
    return searpc_compute_signature ("int", 2, "int", "int");
}


inline static gchar *
searpc_signature_int__int_int_string(void)
{
    return searpc_compute_signature ("int", 3, "int", "int", "string");
}

