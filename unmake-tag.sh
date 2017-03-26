#!/bin/bash

BR=`git rev-parse --abbrev-ref HEAD`
if [ "$BR" != "master" ]; then
    echo "Not on master branch"
    exit 1
fi

REV=`git log -1 | grep merge | sed -e 's/.*merge \([^ ]\+\).*/\1/'`
if [ -z "$REV" ]; then
    echo "No merge commit found"
    exit 1
fi

if ! git diff-files --quiet; then
    echo "Work tree is not clean"
    exit 1
fi

echo "Undoing $REV"
git reset --h HEAD^
git checkout devel
git tag -d $REV
git tag -d $REV-dev
git reset --h HEAD^
