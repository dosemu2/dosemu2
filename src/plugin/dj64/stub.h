#ifndef STUB_H
#define STUB_H

int djstub_main(int argc, char *argv[], char *envp[], unsigned psp_sel,
    cpuctx_t *scp, char *(*SEL_ADR)(uint16_t sel));

#endif
