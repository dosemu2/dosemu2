#ifndef DOSEMU_CONFIG_H
#define DOSEMu_CONFIG_H
typedef void (*config_scrub_t)(void);
int register_config_scrub(config_scrub_t config_scrub);
void unregister_config_scrub(config_scrub_t old_config_scrub);
#endif
