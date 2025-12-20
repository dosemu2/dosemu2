#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/wasmfs.h>
#include <stdlib.h>
#include <assert.h>
#endif
#include "dosemu2/emu.h"

extern char **environ;

int main(int argc, char **argv)
{
#ifdef __EMSCRIPTEN__
    const char *h = emscripten_run_script_string("process.env.HOME");
    const char *h2 = getenv("HOME");
    backend_t home, usr;
    int err;

    assert(h && h2);
    home = wasmfs_create_node_backend(h);
    assert(home);
    usr = wasmfs_create_node_backend("/usr");
    assert(usr);
    err = wasmfs_create_directory("/usr", 0500, usr);
    assert(!err);
    err = mkdir("/home", 0755);
    assert(!err);
    err = wasmfs_create_directory(h, 0700, home);
    assert(!err);
    err = wasmfs_create_directory(h2, 0700, home);
    assert(!err);
#endif
    return dosemu2_emulate(argc, argv, environ);
}
