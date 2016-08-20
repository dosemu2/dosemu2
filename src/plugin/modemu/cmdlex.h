typedef enum {
    CMDST_OK,
    CMDST_ERROR,
    CMDST_CONNECT,
    CMDST_NOCARRIER,
    CMDST_NOAT,
    CMDST_ATD,
    CMDST_ATO
} Cmdstat;

Cmdstat
cmdLex(const char *ptr);
