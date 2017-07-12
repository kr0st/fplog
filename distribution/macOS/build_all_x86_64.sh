#!/bin/bash

echo 'building for x86_64'
cd ..
cd ..

rm -rf ./Stage/*

xcodebuild -scheme fpcollect clean
xcodebuild -scheme fpylog3 clean
xcodebuild -scheme fplogd clean

xcodebuild -scheme fpcollect archive
xcodebuild -scheme fpylog3 archive
xcodebuild -scheme fplogd archive

