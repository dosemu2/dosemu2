#!/bin/sh

if [ $# != 2 ]; then
    echo "Usage: $0 <src_dir> <build_dir>" >&2
    exit 1
fi
DATE=`git -C $1 log -1 --format=%cd --date=rfc`
if [ $? != 0 ]; then
    echo "Non-git builds deprecated" >&2
    exit 1
fi
TSTAMP=$2/.tstamp
if ! touch --date="$DATE" $TSTAMP 2>/dev/null; then
    echo "touch doesnt support --date, build may be incomplete" >&2
    if [ ! -f "$TSTAMP" ]; then
        touch $TSTAMP
    fi
fi
echo $TSTAMP
