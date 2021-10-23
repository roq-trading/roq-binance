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
