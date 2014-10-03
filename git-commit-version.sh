#!/bin/sh

SNV_VERSION=`git show -s --format=%h`
echo -n "3.4.0-Git-$SNV_VERSION"

