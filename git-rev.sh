#!/bin/sh

CDUP=`git rev-parse --show-cdup`
if [ $? != 0 ]; then
    echo "Non-git builds deprecated" >&2
    exit 1
fi
TSTAMP=${CDUP}.tstamp
git log -1 --format=%cd --date=rfc | xargs -I {} touch --date={} $TSTAMP
echo $TSTAMP
