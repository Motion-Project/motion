#!/bin/sh

FILES=./neg/*.jpg
for FULLNAME in $FILES
do
  printf "$FULLNAME \n" >>neglist.txt
done