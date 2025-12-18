#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "dosemu2/emu.h"

int main(int argc, char **argv, char * const *envp)
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
        FS.mkdir('/usr');
        FS.mount(NODEFS, { root: '/usr' }, '/usr');
        FS.mkdir(process.env.HOME);
        FS.mount(NODEFS, { root: process.env.HOME }, process.env.HOME);
        FS.mount(NODEFS, { root: process.env.HOME }, '/home/web_user');
    });
#endif
    return dosemu2_emulate(argc, argv, envp);
}
