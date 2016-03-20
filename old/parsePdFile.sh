#!/bin/bash
#rm controls.txt
#             find $0-control      remove $0             remove trailing semicolons    remove "control", strip number
cat $1 | grep "\$0-control" | sed s/"^.*\$0-"/""/ | sed s/";$"/""/              | sed s/"control\([0-9]\)"/"\1"/   \
|  sed s/"^\([0-9]\) label\:\([^ ]*\)"/"Name\1 \2 \n\1"/ \
| sed s/"^\([0-9]\) range\: \([0-9.]*\) \([0-9.]*\) \([0-9.]*\)"/"control\1minimum \2 \ncontrol\1default \3 \ncontrol\1maximum \4"/ >> replacements.txt



#modify lines starting with a number eg. 5 and range: are converted to control5minimum control5default control5maximum:

#echo "cat pdLV2-stereo.ttl-template |" > replaceTemplate.sh
 #/ | sed s/"\([^ ]*\) \(.*\)"/"sed s\/\"\1\"\/\"\2\"\/ | \\\\"/ >> replaceTemplate.sh 

#echo "cat" >> replaceTemplate.sh
#chmod +x replaceTemplate.sh

#| sed s/"\([^ ]*\) \(.*\)"/"sed s\/\"\1\"\/\"\2\|\\\\\"\/"/

# then run ./process.sh | sed s/"\([^ ]*\) \(.*\)"/"sed s\/\"\1\"\/\2\"/"/ 
# which will create exucutable code to replace names of the template.

# copy the template to the original name, then process several times with sed? or run a large pipe through all changes at once?
