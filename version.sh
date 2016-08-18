#!/bin/sh
SNV_VERSION=`git show -s --format=%h`
echo -n "3.4.1+git$SNV_VERSION"
