.. _roq-binance:

.. |dagger| unicode:: U+2020
.. |double-dagger| unicode:: U+2021
.. |right-arrow| unicode:: U+2192
.. |right-double-arrow| unicode:: U+21D2
.. |left-right-double-arrow| unicode:: U+21D4
.. |check-mark| unicode:: U+2705
.. |cross-mark| unicode:: U+274C
.. |negative-cross-mark| unicode:: U+274E
.. |footnote-1| unicode:: U+2776
.. |footnote-2| unicode:: U+2777
.. |footnote-3| unicode:: U+2778


roq-binance
===========

.. tab:: Unstable

  .. code-block:: shell

     $ conda install \
           --channel https://roq-trading.com/conda/unstable \
           roq-binance

.. tab:: Stable

  .. code-block:: shell

     $ conda install \
           --channel https://roq-trading.com/conda/stable \
           roq-binance


Supports
--------

.. grid::  2
  :gutter: 2

  .. grid-item-card::  Products

    .. list-table::
      :widths: auto
      :align: left

      * - :cpp:enumerator:`Spot <roq::SecurityType::SPOT>`
        - |check-mark|
        -
      * - :cpp:enumerator:`Futures <roq::SecurityType::FUTURES>`
        - |cross-mark|
        -
      * - :cpp:enumerator:`Swap <roq::SecurityType::SWAP>`
        - |cross-mark|
        -
      * - :cpp:enumerator:`Option <roq::SecurityType::OPTION>`
        - |cross-mark|
        -

  .. grid-item-card::  Market Data

    .. list-table::
      :widths: auto
      :align: left

      * - :cpp:class:`ReferenceData <roq::ReferenceData>`
        - |check-mark|
        - |footnote-1|
      * - :cpp:class:`MarketStatus <roq::MarketStatus>`
        - |check-mark|
        - |footnote-1|
      * - :cpp:class:`TopOfBook <roq::TopOfBook>`
        - |check-mark|
        -
      * - :cpp:class:`MarketByPrice <roq::MarketByPriceUpdate>`
        - |check-mark|
        -
      * - :cpp:class:`MarketByOrder <roq::MarketByOrderUpdate>`
        - |cross-mark|
        -
      * - :cpp:class:`TradeSummary <roq::TradeSummary>`
        - |check-mark|
        -
      * - :cpp:class:`Statistics <roq::StatisticsUpdate>`
        - |check-mark|
        -
      * - :cpp:class:`TimeSeries <roq::TimeSeriesUpdate>`
        - |check-mark|
        -

  .. grid-item-card::  Orders & Quotes

    .. list-table::
      :widths: auto
      :align: left

      * - :cpp:class:`CreateOrder <roq::CreateOrder>`
        - |check-mark|
        -
      * - :cpp:class:`ModifyOrder <roq::ModifyOrder>`
        - |cross-mark|
        -
      * - :cpp:class:`CancelOrder <roq::CancelOrder>`
        - |check-mark|
        -
      * - :cpp:class:`CancelAllOrders <roq::CancelAllOrders>`
        - |check-mark|
        -
      * - :cpp:class:`MassQuote <roq::MassQuote>`
        - |cross-mark|
        -
      * - :cpp:class:`CancelQuotes <roq::CancelQuotes>`
        - |cross-mark|
        -

  .. grid-item-card::  Account

    .. list-table::
      :widths: auto
      :align: left

      * - :cpp:class:`Funds <roq::FundsUpdate>`
        - |check-mark|
        -
      * - :cpp:class:`Position <roq::PositionUpdate>`
        - |cross-mark|
        -

.. note::

   |check-mark| = Available.

   |negative-cross-mark| = Not implemented.

   |cross-mark| = Unavailable.

   |footnote-1| The exchange protocol does not support streaming updates for reference data and market status.


Using
-----

.. code-block:: shell

   $ roq-binance [FLAGS]


.. _roq-binance-flags:

Flags
-----

.. code-block:: shell

  $ roq-binance --help

.. tab:: Flags

   .. include:: flags/flags.rstinc

.. tab:: REST

   .. include:: flags/rest.rstinc

.. tab:: WS

   .. include:: flags/ws.rstinc

.. tab:: WS API

   .. include:: flags/ws_api.rstinc

.. tab:: MBP

   .. include:: flags/mbp.rstinc

.. tab:: OMS

   .. include:: flags/oms.rstinc

.. tab:: Request

   .. include:: flags/request.rstinc

.. tab:: Misc

   .. include:: flags/misc.rstinc


Environments
------------

.. tab:: Prod

   .. code-block:: shell

      $ --flagfile $CONDA_PREFIX/share/roq-binance/flags/prod/flags.cfg

   .. include:: flags/prod/flags.cfg
     :code: shell

.. tab:: Test

   .. code-block:: shell

      $ --flagfile $CONDA_PREFIX/share/roq-binance/flags/test/flags.cfg

   .. include:: flags/test/flags.cfg
     :code: shell


Configuration
-------------

.. code-block:: shell

   $ --flagfile $CONDA_PREFIX/share/roq-binance/config.toml

.. important::

   This template will be replaced when the software is upgraded.
   Make a copy and modify to your own needs.

.. include:: config.toml
   :code: toml


Market Data
-----------


Inbound
~~~~~~~

.. tab:: TradingStatus

   .. list-table::
     :header-rows: 1
     :widths: auto
     :align: left

     * - Enum
       -
       -

     * - :code:`TRADING`
       - |right-double-arrow|
       - :cpp:enumerator:`OPEN <roq::TradingStatus::OPEN>`

     * - :code:`HALT`
       - |right-double-arrow|
       - :cpp:enumerator:`HALT <roq::TradingStatus::HALT>`

     * - :code:`BREAK`
       - |right-double-arrow|
       - :cpp:enumerator:`CLOSE <roq::TradingStatus::CLOSE>`

     * - :code:`END_OF_DAY`
       - |right-double-arrow|
       - :cpp:enumerator:`END_OF_DAY <roq::TradingStatus::END_OF_DAY>`

     * - :code:`PRE_TRADING`
       - |right-double-arrow|
       - :cpp:enumerator:`PRE_OPEN <roq::TradingStatus::PRE_OPEN>`

     * - :code:`AUCTION_MATCH`
       - |right-double-arrow|
       - :cpp:enumerator:`PRE_OPEN <roq::TradingStatus::PRE_OPEN>`

     * - :code:`POST_TRADING`
       - |right-double-arrow|
       - :cpp:enumerator:`CLOSE <roq::TradingStatus::CLOSE>`


.. tab:: StatisticsType

   .. list-table::
     :header-rows: 1
     :widths: auto
     :align: left

     * - Event
       - Field
       - Comment
       -
       -

     * - :code:`miniTicker`
       - :code:`o`
       - Open price
       - |right-double-arrow|
       - :cpp:enumerator:`OPEN_PRICE <roq::StatisticsType::OPEN_PRICE>`

     * - :code:`miniTicker`
       - :code:`h`
       - High price
       - |right-double-arrow|
       - :cpp:enumerator:`HIGHEST_TRADED_PRICE <roq::StatisticsType::HIGHEST_TRADED_PRICE>`

     * - :code:`miniTicker`
       - :code:`l`
       - Low price
       - |right-double-arrow|
       - :cpp:enumerator:`LOWEST_TRADED_PRICE <roq::StatisticsType::LOWEST_TRADED_PRICE>`

     * - :code:`miniTicker`
       - :code:`c`
       - Close price
       - |right-double-arrow|
       - :cpp:enumerator:`CLOSE_PRICE <roq::StatisticsType::CLOSE_PRICE>`


Order Management
----------------


Inbound
~~~~~~~

.. tab:: OrderType

   .. list-table::
     :header-rows: 1
     :widths: auto
     :align: left

     * - Enum
       -
       -

     * - :code:`LIMIT`
       - |right-double-arrow|
       - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`

     * - :code:`MARKET`
       - |right-double-arrow|
       - :cpp:enumerator:`MARKET <roq::OrderType::MARKET>`

     * - :code:`STOP_LOSS`
       - |right-double-arrow|
       - :cpp:enumerator:`MARKET <roq::OrderType::MARKET>`

     * - :code:`STOP_LOSS_LIMIT`
       - |right-double-arrow|
       - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`

     * - :code:`TAKE_PROFIT`
       - |right-double-arrow|
       - :cpp:enumerator:`UNDEFINED <roq::OrderType::UNDEFINED>`

     * - :code:`TAKE_PROFIT_LIMIT`
       - |right-double-arrow|
       - :cpp:enumerator:`UNDEFINED <roq::OrderType::UNDEFINED>`

     * - :code:`LIMIT_MAKER`
       - |right-double-arrow|
       - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`


.. tab:: TimeInForce

   .. list-table::
     :header-rows: 1
     :widths: auto
     :align: left

     * - Enum
       -
       -

     * - :code:`GTC`
       - |right-double-arrow|
       - :cpp:enumerator:`GTC <roq::TimeInForce::GTC>`

     * - :code:`IOC`
       - |right-double-arrow|
       - :cpp:enumerator:`IOC <roq::TimeInForce::IOC>`

     * - :code:`FOK`
       - |right-double-arrow|
       - :cpp:enumerator:`FOK <roq::TimeInForce::FOK>`


.. tab:: OrderStatus

   .. list-table::
     :header-rows: 1
     :widths: auto
     :align: left

     * - Enum
       -
       -

     * - :code:`NEW`
       - |right-double-arrow|
       - :cpp:enumerator:`WORKING <roq::OrderStatus::WORKING>`

     * - :code:`PARTIALLY_FILLED`
       - |right-double-arrow|
       - :cpp:enumerator:`WORKING <roq::OrderStatus::WORKING>`

     * - :code:`FILLED`
       - |right-double-arrow|
       - :cpp:enumerator:`COMPLETED <roq::OrderStatus::COMPLETED>`

     * - :code:`CANCELED`
       - |right-double-arrow|
       - :cpp:enumerator:`CANCELED <roq::OrderStatus::CANCELED>`

     * - :code:`PENDING_CANCEL`
       - |right-double-arrow|
       - :cpp:enumerator:`UNDEFINED <roq::OrderStatus::UNDEFINED>`

     * - :code:`REJECTED`
       - |right-double-arrow|
       - :cpp:enumerator:`REJECTED <roq::OrderStatus::REJECTED>`

     * - :code:`EXPIRED`
       - |right-double-arrow|
       - :cpp:enumerator:`EXPIRED <roq::OrderStatus::EXPIRED>`


Outbound
~~~~~~~~

.. tab:: CreateOrder

   .. list-table::
     :header-rows: 1
     :widths: auto
     :align: left

     * - :cpp:member:`order_type <roq::CreateOrder::order_type>`
       - :cpp:member:`execution_instructions <roq::CreateOrder::execution_instructions>`
       - :cpp:member:`price <roq::CreateOrder::price>`
       - :cpp:member:`stop_price <roq::CreateOrder::stop_price>`
       -
       - :code:`type`
       - :code:`price`
       - :code:`stopPrice`

     * - :cpp:enumerator:`MARKET <roq::OrderType::MARKET>`
       -
       - :code:`NaN`
       - :code:`NaN`
       - |right-double-arrow|
       - :code:`MARKET`
       - |cross-mark|
       - |cross-mark|

     * - :cpp:enumerator:`MARKET <roq::OrderType::MARKET>`
       -
       - :code:`NaN`
       - |check-mark|
       - |right-double-arrow|
       - :code:`STOP_LOSS`
       - |cross-mark|
       - |check-mark|

     * - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`
       -
       - |check-mark|
       - :code:`NaN`
       - |right-double-arrow|
       - :code:`LIMIT`
       - |check-mark|
       - |cross-mark|

     * - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`
       - :cpp:enumerator:`PARTICIPATE_DO_NOT_INITIATE <roq::ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE>`
       - |check-mark|
       - :code:`NaN`
       - |right-double-arrow|
       - :code:`LIMIT_MAKER`
       - |check-mark|
       - |cross-mark|

     * - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`
       -
       - |check-mark|
       - |check-mark|
       - |right-double-arrow|
       - :code:`STOP_LOSS_LIMIT`
       - |check-mark|
       - |check-mark|


.. tab:: ModifyOrder

   TBD


.. tab:: CancelOrder

   TBD


.. tab:: CancelAllOrders

   TBD


Template
~~~~~~~~

.. tab:: :code:`create_order`

   .. list-table::
     :header-rows: 1
     :widths: auto

     * - Field
       - Values
       - Comments

     * - :code:`self_trade_prevention_mode`
       - :code:`EXPIRE_TAKER`, :code:`EXPIRE_MAKER`, :code:`EXPIRE_BOTH`
       - Exchange field is :code:`selfTradePreventionMode`


.. tab:: :code:`cancel_order`

   .. list-table::
     :header-rows: 1
     :widths: auto

     * - Field
       - Values
       - Comments

     * - :code:`cancel_restrictions`
       - :code:`ONLY_NEW`, :code:`ONLY_PARTIALLY_FILLED`
       - Exchange field is :code:`cancelRestrictions`

     * - :code:`cancel_replace_mode`
       - :code:`STOP_ON_FAILURE`, :code:`ALLOW_FAILURE`
       - Exchange field is :code:`cancelReplaceMode`


Comments
~~~~~~~~

* It is only possible to download current order status for open orders.
  The implication is that backup procedures must be implemented to reoncile positions in the
  scenario where orders are completely filled during a disconnect.

* The :code:`newClientOrderId` field (used by :code:`CreateOrder`) must conform to the
  :code:`^[\.A-Z\:/a-z0-9_-]{1,36}$` regular expression (ECMAScript).
  This restricts length and character used when supplying the :code:`routing_id` field.

* The exchange will monitor rate-limit usage per IP address.

* Rate-limit usage is quite strict when downloading full order books.
  Due to this constraint, it may take a very long time to initialize all symbols.
  It is therefore **STRONGLY** recommended to reduce the configured number of symbols, e.g.
  :code:`symbols=".*BTC.*"`, or even more specific by using lists.

* It is possible to see gateway warnings about dropped messages caused by order status
  reversal, e.g. seeing :code:`WORKING` after :code:`COMPLETED`.
  This is partly due to REST vs WebSocket and partly due to exchange publishing
  both :code:`NEW` and :code:`FILLED` order update messages for taker orders.

* The online documentation has a note about rejected messages never ping pushed into
  the user data stream.
  This effectively means that there's the risk that you will have to wait for the
  gateway to detect timeout if the REST connection is disconnected right after an
  order request has been sent.

* The exchange API supports a cancel-replace order request.
  This is supported by first placing a cancel order with :code:`is_last=false` followed
  immediately by a create order with :code:`is_last=true`.

  .. note::
     The two orders must share the same symbol.

* Multiple order entry connections (REST or WS) may be supported by specifying a list
  of local network interfaces to bind to.
  The implementation will then round-robin requests between connections already
  in the ready-state.

* There are different end-points depending on the margin-mode.

  * If nothing is specified, the classic margin-mode is selected.
    The end-points are then taken from :code:`--rest_uri` and :code:`--ws_uri`.

  * The new end-points are selected if the toml config has :code:`margin_mode = "portfolio"`.
    The end-points are then taken from :code:`--rest_pm_uri` and :code:`--ws_pm_uri`.

* We do not currently support isolated margin trading.

* Auto-Cancel only available with the REST API.

* WSAPI keys must be ED25519

* PAPI keys must be HMAC / SHA256


References
----------

Common
~~~~~~

* :ref:`Using Conda <tutorial-conda>`
* :ref:`Using Flags <abseil-cpp>`
* :ref:`Gateway Flags <gateway-flags>`
* :ref:`Gateway Config <gateway-config>`

Exchange
~~~~~~~~

* `Website <https://www.binance.com/trade/>`__
* `Documentation <https://www.binance.com/binance-api/>`__
