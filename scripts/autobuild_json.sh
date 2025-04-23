#!/bin/bash

set -e

rm -rf `pwd`/build/*

# 打开json支持
cd `pwd`/build &&
	cmake -DJSON=1 .. &&
	make

cd ..
cp -r `pwd`/src/include `pwd`/lib