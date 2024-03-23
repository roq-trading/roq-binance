.. _roq-binance:

.. |checkmark| unicode:: U+2713

roq-binance
===========

Links
-----

* `Website <https://www.binance.com/en>`__
* `Support <https://www.binance.com/en/support-center>`__
* `Documentation <https://binance-docs.github.io/apidocs/spot/en/>`__



Supports
--------

.. grid::  2
  :gutter: 2

  .. grid-item-card::  Products

    .. list-table::
      :widths: auto

      * - |checkmark|
        - Spot
      * -
        - Futures
      * -
        - Options

  .. grid-item-card::  Market Data

    .. list-table::
      :widths: auto

      * - |checkmark|
        - Reference Data
      * - |checkmark|
        - Market Status
      * - |checkmark|
        - Top of Book
      * - |checkmark|
        - Market by Price (L2)
      * -
        - Market by Order (L3)
      * - |checkmark|
        - Trade Summary
      * - |checkmark|
        - Statistics

  .. grid-item-card::  Order Management

    .. list-table::
      :widths: auto

      * - |checkmark|
        - Create
      * -
        - Modify
      * - |checkmark|
        - Cancel
      * - |checkmark|
        - Cancel All
      * - (|checkmark|)
        - Auto-Cancel

  .. grid-item-card::  Account Management

    .. list-table::
      :widths: auto

      * -
        - Positions
      * - |checkmark|
        - Funds

.. note::

   * Auto-Cancel only available with the REST API.


Installing
----------

* :ref:`Using Conda <tutorial-conda>`

.. tab:: Stable

  .. code-block:: shell

     $ mamba install \
           --channel https://roq-trading.com/conda/stable \
           roq-binance

.. tab:: Unstable

  .. code-block:: shell

     $ mamba install \
           --channel https://roq-trading.com/conda/unstable \
           roq-binance


Using
-----

.. code-block:: shell

   $ roq-binance \
         --name binance \
         --config_file $CONFIG_FILE_PATH \
         --client_listen_address $UNIX_SOCKET_PATH \
         --flagfile $ENVIRONMENT_FLAGFILE


.. _roq-binance-flags:

Flags
-----

* :ref:`Using Flags <abseil-cpp>`
* :ref:`Gateway Flags <gateway-flags>`

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

.. tab:: Download

   .. include:: flags/download.rstinc

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

      $ $CONDA_PREFIX/share/roq-binance/flags/prod/flags.cfg

   .. include:: flags/prod/flags.cfg
     :code: shell

.. tab:: Test

   .. code-block:: shell

      $ $CONDA_PREFIX/share/roq-binance/flags/test/flags.cfg

   .. include:: flags/test/flags.cfg
     :code: shell

.. tab:: Prod (US)

   .. code-block:: shell

      $ $CONDA_PREFIX/share/roq-binance/flags/prod-us/flags.cfg

   .. include:: flags/prod-us/flags.cfg
     :code: shell


Configuration
-------------

* :ref:`Gateway Config <gateway-config>`

.. code-block:: shell

   $ $CONDA_PREFIX/share/roq-binance/config.toml

.. important::

   The template will be replaced when the software is upgraded.
   Make a copy and modify to your needs.

.. include:: config.toml
   :code: toml


Market Data
-----------

.. tab:: Live

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::ReferenceData`
      -
      -
      - Unavailable

    * - :cpp:class:`roq::MarketStatus`
      -
      -
      - Unavailable

    * - :cpp:class:`roq::TopOfBook`
      - MarketData
      - <symbol>@bookTicker
      -

    * - :cpp:class:`roq::MarketByPriceUpdate`
      - MarketData
      - <symbol>@depth@<freq>
      - See :ref:`roq-binance-flags`

    * - :cpp:class:`roq::MarketByOrderUpdate`
      -
      -
      - Unavailable

    * - :cpp:class:`roq::TradeSummary`
      - MarketData
      - <symbol>@trade, <symbol>@aggTrade
      - See :ref:`roq-binance-flags`

    * - :cpp:class:`roq::StatisticsUpdate`
      - MarketData
      - <symbol>@miniTicker
      -

.. tab:: Download

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::ReferenceData`
      - OrderEntry
      - GET /api/v3/exchangeInfo
      -

    * - :cpp:class:`roq::MarketStatus`
      - OrderEntry
      - GET /api/v3/exchangeInfo
      -

    * - :cpp:class:`roq::TopOfBook`
      -
      -
      -

    * - :cpp:class:`roq::MarketByPriceUpdate`
      - OrderEntry
      - GET /api/v3/depth
      - See :ref:`roq-binance-flags`

    * - :cpp:class:`roq::MarketByOrderUpdate`
      -
      -
      -

    * - :cpp:class:`roq::TradeSummary`
      -
      -
      -

    * - :cpp:class:`roq::StatisticsUpdate`
      -
      -
      -


Statistics
~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: auto

  * - Type
    - Comments

  * - :cpp:class:`HIGHEST_TRADED_PRICE`
    - (miniTicker) :code:`high_price`

  * - :cpp:class:`LOWEST_TRADED_PRICE`
    - (miniTicker) :code:`low_price`

  * - :cpp:class:`OPEN_PRICE`
    - (miniTicker) :code:`open_price`

  * - :cpp:class:`CLOSE_PRICE`
    - (miniTicker) :code:`close_price`


Order Management
----------------

.. tab:: Live

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::OrderUpdate`
      - DropCopy
      - executionReport
      -

    * - :cpp:class:`roq::TradeUpdate`
      - DropCopy
      - executionReport
      -

.. tab:: Download

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::OrderUpdate`
      - OrderEntry
      - GET /api/v3/openOrders
      - It is only possible to download **open** orders

    * - :cpp:class:`roq::TradeUpdate`
      -
      -
      -

.. tab:: Request

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::CreateOrder`
      - OrderEntry
      - POST /api/v3/order
      -

    * - :cpp:class:`roq::ModifyOrder`
      -
      -
      - Unavailable

    * - :cpp:class:`roq::CancelOrder`
      - OrderEntry
      - DELETE /api/v3/order
      -

    * - :cpp:class:`roq::CancelAllOrders`
      - OrderEntry
      - DELETE /api/v3/openOrders
      - This request is per-symbol!
        Only executed for those symbols where the gateway has seen order actions or download.

.. tab:: Response

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::OrderAck`
      - OrderEntry
      - /api/v3/order
      -


Order Types
~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: auto

  * - Type
    - Comments

  * - :cpp:class:`MARKET`
    - Mapped to :code:`'MARKET'` (JSON)

  * - :cpp:class:`LIMIT`
    - Mapped to :code:`'LIMIT'` (JSON)


Time in Force
~~~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: auto

  * - Type
    - Comments

  * - :cpp:class:`GTC`
    - Mapped to :code:`'GTC'` (JSON)

  * - :cpp:class:`IOC`
    - Mapped to :code:`'IOC'` (JSON)

  * - :cpp:class:`FOK`
    - Mapped to :code:`'FOK'` (JSON)


Position Effect
~~~~~~~~~~~~~~~

.. note::

  Not supported


Execution Instructions
~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: auto

  * - Type
    - Comments

  * - :cpp:class:`PARTICIPATE_DO_NOT_INITIATE`
    - Maps :code:`OrderType` to :code:`'LIMIT_MAKER'` (JSON)


Templates
~~~~~~~~~

:code:`create_order`
^^^^^^^^^^^^^^^^^^^^

.. list-table::
  :header-rows: 1
  :widths: auto

  * - Field
    - Values
    - Comments

  * - :code:`self_trade_prevention_mode`
    - :code:`EXPIRE_TAKER`, :code:`EXPIRE_MAKER`, :code:`EXPIRE_BOTH`
    - Exchange field is :code:`selfTradePreventionMode`


:code:`cancel_order`
^^^^^^^^^^^^^^^^^^^^

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


Account Management
------------------

.. tab:: Live

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::PositionUpdate`
      -
      -
      - Unavailable

    * - :cpp:class:`roq::FundsUpdate`
      - DropCopy
      - outboundAccountInfo, outboundAccountPosition
      -

.. tab:: Download

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::PositionUpdate`
      -
      -
      - Unavailable

    * - :cpp:class:`roq::FundsUpdate`
      - OrderEntry
      - GET /api/v3/account
      -


Streams
-------

.. tab:: OrderEntry

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Type
      - Comments

    * - REST
      - Primary purpose

        * support order management

        Each connection

        * supports a single account
        * maintains a listen key (used by the DropCopy stream)


.. tab:: DropCopy

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Type
      - Comments

    * - WebSocket
      - Primary purpose

        * live account updates, including orders and fills

        Each connection

        * supports a single account

.. tab:: Marketdata

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Type
      - Comments

    * - WebSocket
      - Primary purpose

        * live market data

        Each connection

        * supports a slice of the symbols

.. tab:: Rest

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Type
      - Comments

    * - REST
      - Primary purpose

        * download reference data

        One connection


Constraints
-----------

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


Comments
--------

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
