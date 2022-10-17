#!/usr/bin/env bash

CWD="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

if [ "$1" == "debug" ]; then
	PREFIX="gdb --args"
else
	PREFIX=
fi

NAME="binance"

CONFIG_FILE="$CWD/config/depdiko.toml"

URI="binance.com"

REST_URI="https://api.$URI"
WS_URI="wss://stream.$URI:9443/stream"

$PREFIX ./roq-binance \
  --name "$NAME" \
  --config_file "$CONFIG_FILE" \
  --event_log_dir "$HOME/var/lib/roq/data" \
  --event_log_symlink \
  --client_listen_address "$HOME/run/$NAME.sock" \
  --metrics_listen_address "$HOME/run/${NAME}_metrics.sock" \
  --ws_uri "$WS_URI" \
  --rest_uri "$REST_URI" \
  $@
