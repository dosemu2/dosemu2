if [ $# != 2 ]; then
    exit 1
fi
if [ -d "$2/m4" ]; then
    AC_F="-I $2/m4 -I $1/m4"
else
    AC_F="-I $1/m4"
fi
autoreconf -v $AC_F
exit $?
