#include "dosemu2/emu.h"

int main(int argc, char **argv, char * const *envp)
{
    return dosemu2_emulate(argc, argv, envp);
}
