#!/bin/bash

set -e

rm -rf `pwd`/build/*
mkdir -p `pwd`/build
cd `pwd`/build &&
	cmake .. &&
	make
cd ..
cp -r `pwd`/src/include `pwd`/lib