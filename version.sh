#!/bin/sh
BASE_VERSION="4.0.1"
if [ -d .git ]; then
	GIT_COMMIT=`git show -s --format=%h`
	printf "$BASE_VERSION+git$GIT_COMMIT"
else
	printf "$BASE_VERSION+gitUNKNOWN"
fi
