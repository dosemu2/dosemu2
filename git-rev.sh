#!/bin/bash

SYM=`git rev-parse --symbolic-full-name HEAD`
GIT_MAJ=`git --version | cut -d "." -f 1 | cut -d " " -f 3`
if [ $GIT_MAJ -lt 2 ]; then
    echo "git version too old" >&2
    REV=".git/$SYM"
else
    REV=`git rev-parse --git-path $SYM`
fi
if [ "${REV:0:1}" != "/" ]; then
    CDUP=`git rev-parse --show-cdup`
    if [ -n "$CDUP" ]; then
	REV=$CDUP$REV
    fi
fi
echo $REV
