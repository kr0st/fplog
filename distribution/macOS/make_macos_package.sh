#! /bin/bash

rm -rf ../../Stage/fplog.framework
mkdir ../../Stage/fplog.framework
mkdir ../../Stage/fplog.framework/Versions
mkdir ../../Stage/fplog.framework/Versions/A
mkdir ../../Stage/fplog.framework/Versions/A/Resources
mkdir ../../Stage/fplog.framework/Versions/A/Headers

cp -r ../../Stage/*dylib ../../Stage/fplog.framework/Versions/A/
cp -r ../../Stage/json.lua ../../Stage/fplog.framework/Versions/A/
cp ../../fplog/Info.plist ../../Stage/fplog.framework/Versions/A/Resources/

cp -r ../../common/fplog_exceptions.h ../../Stage/fplog.framework/Versions/A/Headers/
cp -r ../../common/fplog_transport.h ../../Stage/fplog.framework/Versions/A/Headers/
cp -r ../../fplog/fplog.h ../../Stage/fplog.framework/Versions/A/Headers/
mkdir ../../Stage/fplog.framework/Versions/A/Headers/libjson
cp -r ../../libjson/* ../../Stage/fplog.framework/Versions/A/Headers/libjson


ln -s A ../../Stage/fplog.framework/Versions/Current
ln -s Versions/Current/libfplog.dylib ../../Stage/fplog.framework/fplog
ln -s Versions/Current/Headers ../../Stage/fplog.framework/Headers

./make_fplogd_package.sh

rm -rf ./build
packagesbuild -v ./fplog.pkgproj
productsign --sign "Developer ID Installer: Rostislav Kuratch" ./build/fplog.pkg ../../Stage/fplog.pkg

