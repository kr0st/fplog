#!/bin/bash

rm -rf ../../Stage/dist
rm -rf ../../Stage/fplog.framework

./build_all_x86_64.sh

mkdir ../../Stage/dist/
mkdir ../../Stage/dist/fplog.framework
mkdir ../../Stage/dist/fplog.framework/Versions
mkdir ../../Stage/dist/fplog.framework/Versions/A
mkdir ../../Stage/dist/fplog.framework/Versions/A/Resources
mkdir ../../Stage/dist/fplog.framework/Versions/A/Headers

cp -r ../../Stage/*dylib ../../Stage/dist/fplog.framework/Versions/A/
cp -r ../../Stage/json.lua ../../Stage/dist/fplog.framework/Versions/A/
cp -r ../../Stage/fplog_wrapper.py ../../Stage/dist/fplog.framework/Versions/A/
cp ../../fplog/Info.plist ../../Stage/dist/fplog.framework/Versions/A/Resources/

cp -r ../../common/fplog_exceptions.h ../../Stage/dist/fplog.framework/Versions/A/Headers/
cp -r ../../common/fplog_transport.h ../../Stage/dist/fplog.framework/Versions/A/Headers/
cp -r ../../fplog/fplog.h ../../Stage/dist/fplog.framework/Versions/A/Headers/
mkdir ../../Stage/dist/fplog.framework/Versions/A/Headers/libjson
cp -r ../../libjson/* ../../Stage/dist/fplog.framework/Versions/A/Headers/libjson


ln -s A ../../Stage/dist/fplog.framework/Versions/Current
ln -s Versions/Current/libfplog.dylib ../../Stage/dist/fplog.framework/fplog
ln -s Versions/Current/Headers ../../Stage/dist/fplog.framework/Headers

./make_fplogd_package.sh

rm -rf ./build
packagesbuild -v ./fplog.pkgproj
productsign --sign "Developer ID Installer: Rostislav Kuratch" ./build/fplog.pkg ../../Stage/dist/fplog.pkg
