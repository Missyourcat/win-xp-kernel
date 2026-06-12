#!/bin/sh

sed -i "s|__DB_HOST__|$DB_HOST|g" config.json
sed -i "s|__DB_NAME__|$DB_NAME|g" config.json
sed -i "s|__DB_USER__|$DB_USER|g" config.json
sed -i "s|__DB_PASS__|$DB_PASS|g" config.json

exec ./build/win-xp-kernel