#!/bin/bash

echo "replacing $1 in $2, creating $3"

# replace strings
# usage: replacementsfile sourcefile targetfile
# replacements file: contains lines of 2 words separated by a space. word1 is replaced by word2 of the sourcefile.

if [ $2 = $3 ]; then
echo "Input and Output File are identical"
else
	cp $2 $3
fi

#echo $1
#echo $2
#echo $3

IFSBACKUP=$IFS
IFS=$(echo -en "\n\b")


for i in `cat $1`;
do
  IFS=$IFSBACKUP  
  #echo $i | xargs -n2 printf "sed -i s/\"%s\"/\"%s\"/g $3 \n"
  #SED=`echo $i | xargs -n2 printf "sed -i s/\"%s\"/\"%s\"/g $3 \n"`
  SED2=`echo $i | xargs -n2 printf "s/%s/%s/g"`
  sed -i "$SED2" $3
  IFS=$(echo -en "\n\b")
done;

#  cat $1 |" > replaceTemplate.sh
# | sed s/"\([^ ]*\) \(.*\)"/"sed s\/\"\1\"\/\"\2\"\/ | \\\\"/ >> replaceTemplate.sh 

IFS=$IFSBACKUP

echo "replacements done"