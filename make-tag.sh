#!/bin/sh

if [ -z "$1" ]; then
    exit 1
fi

SUFF=${0: -4}
if [ "$SUFF" != ".tmp" ]; then
    # need to create a temporary version of the script so that
    # changing the branch does not destroy it
    TMP="$0.tmp"
    cp $0 $TMP
    exec $TMP $@
fi

SUBV="$1"
VER="dosemu2-$SUBV"
REV=`git rev-list HEAD ^dosemu-1.5.0 --count`
git stash
echo $SUBV > VERSION
echo $REV >> VERSION
git commit -a -m "update version to $SUBV"
if [ $? != 0 ]; then
    exit 1
fi
git tag -f -a $VER-dev -m "tag devel $VER"
git checkout master
git merge --no-ff --log -m "merge $SUBV release from devel" devel
git tag -f -a $VER -m "tag release $VER"

# remove temporary script
rm $0
