#!/bin/sh

SNV_VERSION=`cd "$1" && LC_ALL=C svn info 2> /dev/null | grep Revision | cut -d' ' -f2`
test $SNV_VERSION || SNV_VERSION=`cd "$1" && grep revision .svn/entries 2>/dev/null | cut -d '"' -f2`
test $SNV_VERSION || SNV_VERSION=UNKNOWN
echo -n "3.2.12"
