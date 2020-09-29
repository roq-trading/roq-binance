#!/usr/bin/env bash

CWD="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

if [ "$1" == "debug" ]; then
	PREFIX="libtool --mode=execute gdb --args"
else
	PREFIX=
fi

NAME="binance-us"

CONFIG_FILE="$CWD/config/$NAME.toml"

URI="binance.us"

REST_URI="https://api.$URI"
WS_URI="wss://stream.$URI:9443/stream"

echo "$WS_URI"

$PREFIX ./roq-binance \
	--name "$NAME" \
	--config-file "$CONFIG_FILE" \
	--client-listen-address $CWD/$NAME.sock \
	--metrics-listen-address $CWD/${NAME}_metrics.sock \
	--ws-uri "$WS_URI" \
	--rest-uri "$REST_URI" \
	$@
