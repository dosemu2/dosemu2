#!/bin/sh

CDUP=`git rev-parse --show-cdup`
if [ $? != 0 ]; then
    echo "Non-git builds deprecated" >&2
    exit 1
fi
TSTAMP=${CDUP}.tstamp
DATE=`git log -1 --format=%cd --date=rfc`
if ! touch --date="$DATE" $TSTAMP 2>/dev/null; then
    # fall-back for macos-X where --date doesn't work
    touch $TSTAMP
fi
echo $TSTAMP
