#!/usr/bin/env bash

if [ "$1" == "debug" ]; then
  PREFIX="gdb --args"
else
  PREFIX=
fi

NAME="binance"

CONFIG="${CONFIG:-$NAME}"

CONFIG_FILE="$ROQ_CONFIG_PATH/roq-binance-2/$CONFIG.toml"

FLAGFILE="../../../share/flags/prod/flags.cfg"

DOWNLOAD_SYMBOLS="BTCUSDT,ETHUSDT"
DOWNLOAD_TRADES_LOOKBACK="60m"

WS_API=true

$PREFIX ./roq-binance-2 \
  --name "$NAME" \
  --config_file "$CONFIG_FILE" \
  --flagfile "$FLAGFILE" \
  --cache_dir "$HOME/var/lib/roq/cache" \
  --event_log_dir "$HOME/var/lib/roq/data" \
  --event_log_symlink true \
  --client_listen_address "$HOME/run/$NAME.sock" \
  --service_listen_address "$HOME/run/metrics/${NAME}.sock" \
  --download_symbols="$DOWNLOAD_SYMBOLS" \
  --download_trades_lookback="$DOWNLOAD_TRADES_LOOKBACK" \
  --ws_api=$WS_API \
  $@
