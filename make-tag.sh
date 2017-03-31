#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 2.0pre20"
    exit 1
fi

SUFF=${0: -4}
if [ "$SUFF" != ".tmp" ]; then
    # need to create a temporary version of the script so that
    # changing the branch does not destroy it
    TMP="/tmp/$0.tmp"
    cp $0 $TMP
    exec $TMP $@
fi

SUBV="$1"
shift
if [ "$1" = "-d" ]; then
    DEV_ONLY=1
    shift
fi
#VER="dosemu2-$SUBV"
VER="$SUBV"
REV=`git rev-list HEAD ^dosemu-1.5.0 --count`
echo $SUBV > VERSION
echo $REV >> VERSION
git commit VERSION -m "update version to $SUBV"
if [ $? != 0 ]; then
    exit 1
fi
git tag -f -a $VER-dev -m "tag devel $VER"
if [ -n "$DEV_ONLY" ]; then
    exit 0
fi
git stash
MWT=`git worktree list --porcelain | grep -B 3 "heads/master" | grep worktree \
	|cut -d " " -f 2`
if [ -n "$MWT" ]; then
    # unfortunately git does not allow checking out the branch that
    # has a work-tree elsewhere
    cd "$MWT"
    git stash
else
    git checkout master
    if [ $? != 0 ]; then
	echo Failure! Undoing...
	git tag -d $VER-dev
	git reset --h HEAD^
	exit 1
    fi
fi
git merge --no-ff --log -m "merge $SUBV release from devel" devel
git tag -f -a $VER -m "tag release $VER"

# remove temporary script
rm $0
