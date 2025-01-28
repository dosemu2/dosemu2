
inline static gchar *
searpc_signature_int__int64_int64_int_int_int64(void)
{
    return searpc_compute_signature ("int", 5, "int64", "int64", "int", "int", "int64");
}


inline static gchar *
searpc_signature_int__int64_int64_int(void)
{
    return searpc_compute_signature ("int", 3, "int64", "int64", "int");
}


inline static gchar *
searpc_signature_int__void(void)
{
    return searpc_compute_signature ("int", 0);
}


inline static gchar *
searpc_signature_int__int(void)
{
    return searpc_compute_signature ("int", 1, "int");
}


inline static gchar *
searpc_signature_int__int_int(void)
{
    return searpc_compute_signature ("int", 2, "int", "int");
}


inline static gchar *
searpc_signature_int__int_int64(void)
{
    return searpc_compute_signature ("int", 2, "int", "int64");
}

