#!/bin/bash

touch ~/fplogd.ini

if [ -s ~/fplogd.ini ]
then
    echo "" > /dev/null
else
    echo "[misc]" > ~/fplogd.ini
    echo "hostname=auto" >> ~/fplogd.ini
    echo "batch_size=31" >> ~/fplogd.ini
    echo "max_queue_size=21000000" >> ~/fplogd.ini
    echo "emergency_timeout=30000" >> ~/fplogd.ini
    echo "emergency_algo=remove_newest_below_prio" >> ~/fplogd.ini
    echo "emergency_fallback_algo=remove_newest" >> ~/fplogd.ini
    echo "emergency_prio=warning" >> ~/fplogd.ini

    echo ";Setting the transport of log messages from fplogd to fpcollect." >> ~/fplogd.ini
    echo "[transport]" >> ~/fplogd.ini
    echo "type=ip" >> ~/fplogd.ini
    echo "transport=udp" >> ~/fplogd.ini
    echo "protocol=sprot" >> ~/fplogd.ini
    echo "ip=127.0.0.1" >> ~/fplogd.ini
    echo "uid=18751_18752" >> ~/fplogd.ini

    echo ";All subscribed apps, i.e. apps sending logs to fplogd." >> ~/fplogd.ini
    echo "[channels]" >> ~/fplogd.ini
    echo "fplog_test=18749_18750" >> ~/fplogd.ini
    echo "fplog_testapp=18849_18850" >> ~/fplogd.ini
fi
