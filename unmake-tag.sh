#!/usr/bin/env bash

MWT=`git worktree list --porcelain | grep -B 3 "heads/master" | grep worktree \
	|cut -d " " -f 2`
if [ -n "$MWT" ]; then
    # unfortunately git does not allow checking out the branch that
    # has a work-tree elsewhere
    pushd "$MWT"
else
    BR=`git rev-parse --abbrev-ref HEAD`
    if [ "$BR" != "master" ]; then
	echo "Not on master branch"
	exit 1
    fi
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
if [ -n "$MWT" ]; then
    popd
fi
git tag -d $REV
git tag -d $REV-dev
if ! git checkout devel; then
    echo "Cannot checkout devel"
    exit 1
fi
git reset --h HEAD^
