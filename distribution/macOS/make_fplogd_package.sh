#! /bin/bash

rm -rf ../../Stage/fplogd.app

mkdir ../../Stage/fplogd.app
mkdir ../../Stage/fplogd.app/Contents
mkdir ../../Stage/fplogd.app/Contents/MacOS

cp -r ../../Stage/fplogd ../../Stage/fplogd.app/Contents/MacOS/
cp -r ../../fplogd/Info.plist ../../Stage/fplogd.app/Contents/

install_name_tool -add_rpath "/Library/Frameworks/fplog.framework/Versions/A/" ../../Stage/fplogd.app/Contents/MacOS/fplogd

codesign -s "Developer ID Application: Rostislav Kuratch" -f ../../Stage/fplogd.app

