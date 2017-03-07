#!/bin/bash

SYM=`git rev-parse --symbolic-full-name HEAD`
REV=`git rev-parse --git-path $SYM`
if [ "${REV:0:1}" != "/" ]; then
    CDUP=`git rev-parse --show-cdup`
    if [ -n "$CDUP" ]; then
	REV=$CDUP$REV
    fi
fi
echo $REV
