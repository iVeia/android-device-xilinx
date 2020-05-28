#!/bin/bash

md5=`md5sum cups.sh | awk '{ print $1 }'`
echo $md5sum
echo "cups:script:none:0:md5:$md5" > manifest

