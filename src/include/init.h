#ifndef DOSEMU_INIT_H
#define DOSEMU_INIT_H

#define CONSTRUCTOR(X) X __attribute__((constructor)); X
#define DESTRUCTOR(X) X __attribute__((destructor)); X

#ifdef USE_DL_PLUGINS
#define CONSTRUCTOR2(x) CONSTRUCTOR(static void x(void))
#define __S(x) #x
#define _S(x) __S(x)
#define LOAD_PLUGIN(x) load_plugin(_S(x))
#else
#define CONSTRUCTOR2(x) void plugin_##x(void); void plugin_##x(void)
#define LOAD_PLUGIN(x) void plugin_##x(void); plugin_##x()
#endif

#endif /* DOSEMU_INIT_H */
