# ReferenceData

## SPOT

```
{
  symbol="BTCUSDT",
  status=TRADING,
  base_asset="BTC",
  base_asset_precision=8,
  quote_asset="USDT",
  quote_precision=8,
  quote_asset_precision=8,
  base_commission_precision=8,
  quote_commission_precision=8,
  iceberg_allowed=true,
  oco_allowed=true,
  quote_order_qty_market_allowed=true,
  is_spot_trading_allowed=true,
  is_margin_trading_allowed=true,
  filters="[{
    "filterType":"PRICE_FILTER",
    "minPrice":"0.01000000",
    "maxPrice":"1000000.00000000",
    "tickSize":"0.01000000"
  }, {
    "filterType":"LOT_SIZE",
    "minQty":"0.00001000",
    "maxQty":"9000.00000000",
    "stepSize":"0.00001000"
  }, {
    "filterType":"ICEBERG_PARTS",
    "limit":10
  }, {
    "filterType":"MARKET_LOT_SIZE",
    "minQty":"0.00000000",
    "maxQty":"120.04122288",
    "stepSize":"0.00000000"
  }, {
    "filterType":"TRAILING_DELTA",
    "minTrailingAboveDelta":10,
    "maxTrailingAboveDelta":2000,
    "minTrailingBelowDelta":10,
    "maxTrailingBelowDelta":2000
  }, {
    "filterType":"PERCENT_PRICE_BY_SIDE",
    "bidMultiplierUp":"5",
    "bidMultiplierDown":"0.2",
    "askMultiplierUp":"5",
    "askMultiplierDown":"0.2",
    "avgPriceMins":5
  }, {
    "filterType":"NOTIONAL",
    "minNotional":"5.00000000",
    "applyMinToMarket":true,
    "maxNotional":"9000000.00000000",
    "applyMaxToMarket":false,
    "avgPriceMins":5
  }, {
    "filterType":"MAX_NUM_ORDERS",
    "maxNumOrders":200
  }, {
    "filterType":"MAX_NUM_ALGO_ORDERS",
    "maxNumAlgoOrders":5
  }
  ]",
  allow_trailing_stop=true,
  cancel_replace_allowed=true,
  default_self_trade_prevention_mode="EXPIRE_MAKER",
  oto_allowed=true
}
```



There's a limit on # of subscriptions -- seems to be around 1k
Streams (ws) require lower case symbols -- everywhere else it's uppercase



MAKER
```
body="symbol=LTCUSDT&side=BUY&type=LIMIT&timeInForce=GTC&quantity=0.100&price=193.4&newClientOrderId=JQAC6QMAAQAAv957tzEX&recvWindow=5000"
status=OK, body="{"symbol":"LTCUSDT","orderId":2428636529,"orderListId":-1,"clientOrderId":"JQAC6QMAAQAAv957tzEX","transactTime":1634961499306,"price":"193.40000000","origQty":"0.10000000","executedQty":"0.00000000","cummulativeQuoteQty":"0.00000000","status":"NEW","timeInForce":"GTC","type":"LIMIT","side":"BUY","fills":[]}"
message="{"stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX","data":{"e":"executionReport","E":1634961499306,"s":"LTCUSDT","c":"JQAC6QMAAQAAv957tzEX","S":"BUY","o":"LIMIT","f":"GTC","q":"0.10000000","p":"193.40000000","P":"0.00000000","F":"0.00000000","g":-1,"C":"","x":"NEW","X":"NEW","r":"NONE","i":2428636529,"l":"0.00000000","z":"0.00000000","L":"0.00000000","n":"0","N":null,"T":1634961499306,"t":-1,"I":5052314028,"w":true,"m":false,"M":false,"O":1634961499306,"Z":"0.00000000","Y":"0.00000000","Q":"0.00000000"}}"
message="{"stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX","data":{"e":"outboundAccountPosition","E":1634961499306,"u":1634961499306,"B":[{"a":"LTC","f":"0.00000000","l":"0.00000000"},{"a":"BNB","f":"0.00023143","l":"0.00000000"},{"a":"USDT","f":"1.80364261","l":"19.34000000"}]}}"
message="{"stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX","data":{"e":"executionReport","E":1634961663038,"s":"LTCUSDT","c":"JQAC6QMAAQAAv957tzEX","S":"BUY","o":"LIMIT","f":"GTC","q":"0.10000000","p":"193.40000000","P":"0.00000000","F":"0.00000000","g":-1,"C":"","x":"TRADE","X":"FILLED","r":"NONE","i":2428636529,"l":"0.10000000","z":"0.10000000","L":"193.40000000","n":"0.00003014","N":"BNB","T":1634961663038,"t":207223058,"I":5052325712,"w":false,"m":true,"M":true,"O":1634961499306,"Z":"19.34000000","Y":"19.34000000","Q":"0.00000000"}}"
message="{"stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX","data":{"e":"outboundAccountPosition","E":1634961663038,"u":1634961663038,"B":[{"a":"LTC","f":"0.10000000","l":"0.00000000"},{"a":"BNB","f":"0.00020129","l":"0.00000000"},{"a":"USDT","f":"1.80364261","l":"0.00000000"}]}}"
```

TAKER
```
body="symbol=LTCUSDT&side=SELL&type=LIMIT&timeInForce=GTC&quantity=0.100&price=193.8&newClientOrderId=swAC6QMAAQAAiLvZ0TEX&recvWindow=5000"
status=OK, body="{"symbol":"LTCUSDT","orderId":2428648201,"orderListId":-1,"clientOrderId":"swAC6QMAAQAAiLvZ0TEX","transactTime":1634961941676,"price":"193.80000000","origQty":"0.10000000","executedQty":"0.10000000","cummulativeQuoteQty":"19.38000000","status":"FILLED","timeInForce":"GTC","type":"LIMIT","side":"SELL","fills":[{"price":"193.80000000","qty":"0.10000000","commission":"0.00003016","commissionAsset":"BNB","tradeId":207223511}]}"
message="{"stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX","data":{"e":"executionReport","E":1634961941676,"s":"LTCUSDT","c":"swAC6QMAAQAAiLvZ0TEX","S":"SELL","o":"LIMIT","f":"GTC","q":"0.10000000","p":"193.80000000","P":"0.00000000","F":"0.00000000","g":-1,"C":"","x":"NEW","X":"NEW","r":"NONE","i":2428648201,"l":"0.00000000","z":"0.00000000","L":"0.00000000","n":"0","N":null,"T":1634961941676,"t":-1,"I":5052338230,"w":true,"m":false,"M":false,"O":1634961941676,"Z":"0.00000000","Y":"0.00000000","Q":"0.00000000"}}"
>>> Received non-final order status WORKING after order has reached final status COMPLETED
message="{"stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX","data":{"e":"executionReport","E":1634961941676,"s":"LTCUSDT","c":"swAC6QMAAQAAiLvZ0TEX","S":"SELL","o":"LIMIT","f":"GTC","q":"0.10000000","p":"193.80000000","P":"0.00000000","F":"0.00000000","g":-1,"C":"","x":"TRADE","X":"FILLED","r":"NONE","i":2428648201,"l":"0.10000000","z":"0.10000000","L":"193.80000000","n":"0.00003016","N":"BNB","T":1634961941676,"t":207223511,"I":5052338231,"w":false,"m":false,"M":true,"O":1634961941676,"Z":"19.38000000","Y":"19.38000000","Q":"0.00000000"}}"
*** CHANGE(TRADED QUANTITY 0.1 ==> 0.1) (0) != LAST TRADED QUANTITY (0.1) ***
*** RESET LAST TRADED ***
message="{"stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX","data":{"e":"outboundAccountPosition","E":1634961941676,"u":1634961941676,"B":[{"a":"LTC","f":"0.00000000","l":"0.00000000"},{"a":"BNB","f":"0.00017113","l":"0.00000000"},{"a":"USDT","f":"21.18364261","l":"0.00000000"}]}}"
```
