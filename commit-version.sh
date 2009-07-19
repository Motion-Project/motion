#!/bin/sh

SNV_VERSION=`cd "$1" && LC_ALL=C svn info 2> /dev/null | grep Revision | cut -d' ' -f2`
SNV_VERSION=`expr $SNV_VERSION + 1`
echo -n "trunk-r$SNV_VERSION"
