#! /bin/sh

abspath() {
	case $1 in
		/*) echo "$1" ;;
		*) echo `pwd`/"$1" ;;
	esac
}

TOP=$(abspath "`dirname $0`/..")
PLUG=plugin
SRCDIR=$TOP/src/$PLUG
DESTDIR=src/$PLUG
CONF=config
CONFIGURE=configure.ac

dir=$1
on=$2
shift
shift
req_on=$on
DSTDIR=$DESTDIR/$dir
if [ "$on" = "yes" -a -f $SRCDIR/$dir/$CONFIGURE ]; then
	wd=`pwd`
	[ -d $DSTDIR ] || mkdir -p $DSTDIR
	cd $DSTDIR
	[ -f $CONFIGURE ] || ln -s $SRCDIR/$dir/$CONFIGURE $CONFIGURE
	[ -f Makefile.conf.in ] || [ ! -f $SRCDIR/$dir/Makefile.conf.in ] || \
			ln -s $SRCDIR/$dir/Makefile.conf.in Makefile.conf.in
	echo "=== configuring in $dir"
	trap "echo ; exit 130" INT
	if [ ! -f ./configure ] && ! $TOP/scripts/aconf.sh $TOP $SRCDIR/$dir; then
		on="no"
	fi
	if [ ! -f ./configure ]; then
		on="no"
	else
		eval OPTS=\$${dir}_OPTS
		echo ./configure $OPTS
		if ! ./configure $OPTS; then
			echo "Configuration for $dir failed, disabling"
			on="no"
		fi
	fi
	trap - INT
	cd $wd
fi
if test -d $SRCDIR/$dir ; then
	mkdir -p $DSTDIR/$CONF
	echo $on > $DSTDIR/$CONF/plugin_enable
fi
if [ "$req_on" != "$on" ]; then
	exit 1
fi
