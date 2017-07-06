#!/bin/bash

echo 'building for x86_32'
cd ..
cd ..
cd ./codelite

rm -rf ../Stage/*

codelite-make -w fplog.workspace -p fpcollect -c Release-32bit -d rebuild -e
codelite-make -w fplog.workspace -p fplogd -c Release-32bit -d rebuild -e
codelite-make -w fplog.workspace -p fplog_test -c Release-32bit -d rebuild -e
codelite-make -w fplog.workspace -p spipc_test -c Release-32bit -d rebuild -e

