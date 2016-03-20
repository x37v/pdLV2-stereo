cat pdLV2-stereo.ttl-template |
sed s/"Name5"/"Vibrato-Depth "/ | \
sed s/"control5minimum"/"0 "/ | \
sed s/"control5default"/"20 "/ | \
sed s/"control5maximum"/"100"/ | \
sed s/"Name4"/"Vibrato-Freq "/ | \
sed s/"control4minimum"/"0 "/ | \
sed s/"control4default"/"20 "/ | \
sed s/"control4maximum"/"100"/ | \
sed s/"Name3"/"Smoothing "/ | \
sed s/"control3minimum"/"0.5 "/ | \
sed s/"control3default"/"20 "/ | \
sed s/"control3maximum"/"1000"/ | \
sed s/"Name2"/"Amplitude "/ | \
sed s/"control2minimum"/"0 "/ | \
sed s/"control2default"/"0.2 "/ | \
sed s/"control2maximum"/"1"/ | \
sed s/"Name1"/"Fooequency "/ | \
sed s/"control1minimum"/"0 "/ | \
sed s/"control1default"/"440 "/ | \
sed s/"control1maximum"/"20000"/ | \
cat
