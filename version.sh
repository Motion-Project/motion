#!/bin/sh

SNV_VERSION=`cd "$1" && LC_ALL=C svn info 2> /dev/null | grep Revision | cut -d' ' -f2`
test $SNV_VERSION || SNV_VERSION=`cd "$1" && grep revision .svn/entries 2>/dev/null | cut -d '"' -f2`
test $SNV_VERSION || SNV_VERSION=UNKNOWN
GITDIR=".git"
if [ -d "$GITDIR" ]; then 
  SNV_VERSION=`git show -s --format=%h`
fi
echo -n "Unofficial-Git-$SNV_VERSION"

