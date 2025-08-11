#!/usr/bin/env bash

if [ "$1" == "debug" ]; then
  PREFIX="gdb --args"
else
  PREFIX=
fi

NAME="binance"

CONFIG="${CONFIG:-$NAME}"

CONFIG_FILE="$ROQ_CONFIG_PATH/roq-binance/$CONFIG.toml"

URI="binance.com"

REST_URI="https://api.$URI"
WS_URI="wss://stream.$URI:9443/stream"

REST_PM_URI="https://papi.$URI"                                                                                         
WS_PM_URI="wss://fstream.$URI/pm/ws"

$PREFIX ./roq-binance \
  --name "$NAME" \
  --config_file "$CONFIG_FILE" \
  --cache_dir "$HOME/var/lib/roq/cache" \
  --event_log_dir "$HOME/var/lib/roq/data" \
  --event_log_symlink true \
  --client_listen_address "$HOME/run/$NAME.sock" \
  --service_listen_address "$HOME/run/metrics/${NAME}.sock" \
  --ws_uri "$WS_URI" \
  --rest_uri "$REST_URI" \
  --ws_api=false \
  --ws_pm_uri "$WS_PM_URI" \
  --rest_pm_uri "$REST_PM_URI" \
  --download_symbols="BTCUSDT,ETHUSDT" \
  --download_trades_lookback=0s \
  --cache_all_reference_data=true \
  --download_time_series_lookback "0s" \
  $@
