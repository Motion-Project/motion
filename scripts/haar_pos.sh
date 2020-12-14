#!/bin/sh

printf  "Start with creating positive images movie as input file.\n"
printf  "Run with picture_output_motion roi\n"
printf  "Remove any pictures that are not desired.\n"
printf  "Run this script to create the positive file list\n"
printf  "Run while in the directory with the positive images.\n"
printf  " opencv_createsamples -vec posfiles.vec -info poslist.txt -num 350 -h 96 -w 96 \n"
printf  "\n"
printf  "Next get a sample file that does not have desired positives. \n"
printf  "Change to picture_output on and turn off motion roi \n"
printf  "Change threshold to tiny number and run to create negatives \n"
printf  " opencv_traincascade -data ./model -vec ./pos/posfiles.vec -bg neglist.txt -numPos 350 -numNeg 325 \n" 

FILES=./pos/*.jpg
for FULLNAME in $FILES
do
  HW=$(identify -format '%w %h' $FULLNAME)
  printf "$FULLNAME 1 0 0 $HW \n" >>poslist.txt
done

