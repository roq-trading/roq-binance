# Change Log

All notable changes will be documented in this file.

## Head

## 1.1.1 &ndash; 20251214

## 1.1.0 &ndash; 2025-11-22

## 1.0.9 &ndash; 2025-09-26

### Fixed

* `TradeSummary` had incorrect `side` (#527)
* HTTP response with status code 503 (service unavailable) should map to `RequestStatus::REJECTED` (#522)
* New event-type "liabilityChange" (#519)

## 1.0.8 &ndash; 2025-08-16

## 1.0.7 &ndash; 2025-07-02

### Changed

* Download cross margin borrowed on timer (#510)

### Added

* New flag to control the side-effect for margin trading (#501)
* Support SAPI (#499)

## 1.0.6 &ndash; 2025-05-16

## 1.0.5 &ndash; 2025-03-26

## 1.0.4 &ndash; 2024-12-30

## 1.0.3 &ndash; 2024-11-26

## 1.0.2 &ndash; 2024-07-14

## 1.0.1 &ndash; 2024-04-14

### Added

* PAPI support (#454)

## 1.0.0 &ndash; 2024-03-16

## 0.9.9 &ndash; 2024-01-28

### Fixed

* WS API did not correctly encode the wider `order_id` into the request id (#427)

## 0.9.8 &ndash; 2023-11-20

## 0.9.7 &ndash; 2023-09-18

## 0.9.6 &ndash; 2023-07-22

## 0.9.5 &ndash; 2023-06-12

### Fixed

* Uncaught exception when client disconnects (#363)

## 0.9.4 &ndash; 2023-05-04

### Added

* Support `cancelRestrictions` (#328)

### Fixed

* Possible `CancelOrder` race-condition (#333)

### Changed

* `ReferenceData.security_type` now populated (#342)

## 0.9.3 &ndash; 2023-03-20

### Added

* Capture external trades (#320)

## 0.9.2 &ndash; 2023-02-22

### Added

* Support primary / secondary market data streams (#315)

## 0.9.1 &ndash; 2023-01-12

## 0.9.0 &ndash; 2022-12-22

## 0.8.9 &ndash; 2022-11-14

### Fixed

* OrderAck was missing when HTTP response was HTML (instead of JSON) (#293)

## 0.8.8 &ndash; 2022-10-04

### Added

* Support for 1-shot cancel-replace request (this is *not* modify order!)

## 0.8.7 &ndash; 2022-08-22

### Fixed

* Issues with `LIMIT_MAKER` orders (#257)

### Changed

* Added support for `ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE`

### Added

* New flag `--mbp_max_depth` (#265)

## 0.8.6 &ndash; 2022-07-18

## 0.8.5 &ndash; 2022-06-06

### Changed

* Market data support for `--net_disconnect_on_idle_timeout`.

## 0.8.4 &ndash; 2022-05-14

### Changed

* `TopOfBook.exchange_sequence` now being populated (#205)

## 0.8.3 &ndash; 2022-03-22

### Changed

* Download orders (#39)

### Fixed

* Invalid client order id's when using `routing_id` (#183)

## 0.8.2 &ndash; 2022-02-18

## 0.8.1 &ndash; 2022-01-16

## 0.8.0 &ndash; 2022-01-12

## 0.7.9 &ndash; 2021-12-08

## 0.7.8 &ndash; 2021-11-02

### Added

* Add exchange sequence number to `MarketByPrice` and `MarketByOrder` (#101)
* Add `max_trade_vol` and `trade_vol_step_size` to ReferenceData (#100)

### Changed

* Move cache utilities to API (#111)
* Interface to support binary data from web::socket
* ReferenceData currencies should follow FX conventions (#99)
* Replace `snapshot` (bool) with `update_type` (UpdateType) (#97)
* Moved signature handling to tools library (chore)

### Removed

* Remove custom literals (#110)
* Remove external rate-limiter mirroring from the REST connection (#83)

## 0.7.7 &ndash; 2021-09-20

### Changed

* Added HTTP `request_id` (#55)
* Use `string_buffer` + `std::back_inserter` instead of `string_builder` (#53)

## 0.7.6 &ndash; 2021-09-02

### Changed

* New order management interface (#25)

## 0.7.5 &ndash; 2021-08-08

## 0.7.4 &ndash; 2021-07-20

## 0.7.3 &ndash; 2021-07-06

## 0.7.2 &ndash; 2021-06-20

## 0.7.1 &ndash; 2021-05-30

## 0.7.0 &ndash; 2021-04-15

### Added

* Multi-account support

### Changed

* Streams to support load-balancing

## 0.6.1 &ndash; 2021-02-19

## 0.6.0 &ndash; 2021-02-02

### Changed

* Fixed quote/base currency (ReferenceData)
* Fixed tick\_size and min\_trade\_vol (ReferenceData)

## 0.5.0 &ndash; 2020-12-04

## 0.4.5 &ndash; 2020-11-09

## 0.4.4 &ndash; 2020-09-20

## 0.4.3 &ndash; 2020-09-02

## 0.4.2 &ndash; 2020-07-27

### Removed

* Automake support

## 0.4.1 &ndash; 2020-07-17

## 0.4.0 &ndash; 2020-06-30

## 0.3.9 &ndash; 2020-06-09

## 0.3.8 &ndash; 2020-06-06

## 0.3.7 &ndash; 2020-05-27

## 0.3.6 &ndash; 2020-05-02

## 0.3.5 &ndash; 2020-04-22

## 0.3.4 &ndash; 2020-04-08

## 0.3.3 &ndash; 2020-03-04
