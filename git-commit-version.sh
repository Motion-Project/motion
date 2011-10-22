#!/bin/sh

SNV_VERSION=`git show | grep commit|cut -d' ' -f2`
echo -n "Git-$SNV_VERSION"

