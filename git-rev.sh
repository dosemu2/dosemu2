#!/bin/sh

if [ $# != 1 ]; then
    echo "Usage: $0 <build_dir>" >&2
    exit 1
fi
DATE=`git log -1 --format=%cd --date=rfc`
if [ $? != 0 ]; then
    echo "Non-git builds deprecated" >&2
    exit 1
fi
TSTAMP=$1/.tstamp
if ! touch --date="$DATE" $TSTAMP 2>/dev/null; then
    # fall-back for macos-X where --date doesn't work
    touch $TSTAMP
fi
echo $TSTAMP
