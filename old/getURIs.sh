#!/bin/sh
#grep -o "pluginURI:.*;" $1 | sed s/";$"/""/ | sed s/"pluginURI: "/"templateURI "/ | sed s/"\/"/"\\\\\/"/g > $2
grep -o "pluginURI:.*;" $1 | sed s/";$"/""/ | sed s/"pluginURI: "/"templateURI "/ | sed s/"[\/\.\:]"/"\\\\\\\\\0"/g > $2
