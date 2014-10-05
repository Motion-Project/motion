#!/bin/sh

SNV_VERSION=`cd "$1" && LC_ALL=C svn info 2> /dev/null | grep Revision | cut -d' ' -f2`
test $SNV_VERSION || SNV_VERSION=`cd "$1" && grep revision .svn/entries 2>/dev/null | cut -d '"' -f2`
test $SNV_VERSION || SNV_VERSION=UNKNOWN
SNV_VERSION=`git show -s --format=%h`
echo -n "3.4.0-Git-$SNV_VERSION"

