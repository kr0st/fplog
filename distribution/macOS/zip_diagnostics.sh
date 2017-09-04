#!/bin/bash

7za > /dev/null
if [ $? -ne 0 ]; then
echo "********************* ERROR *********************"
echo "Please install 7zip (p7zip-full package)"
echo "*************************************************"
exit
fi

7za a fplog_diagnostics.7z ../../Stage/*.txt
7za a fplog_diagnostics.7z ../../Stage/*.xml
7za a fplog_diagnostics.7z ../../Stage/*.log

rm -rf ../../Stage/fplog_diagnostics*

mv ./fplog_diagnostics.7z ../../Stage
