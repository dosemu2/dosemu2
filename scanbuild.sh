OUTDIR=/tmp/dosemu2-scan
make -j 9 clean
CC=clang ./default-configure $*
scan-build --use-cc=clang -o $OUTDIR make -j 9
scan-view $OUTDIR/`ls $OUTDIR | tail -n 1`
