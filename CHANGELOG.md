# Changelog

## [1.0.0](https://github.com/embedded-pro/can-lite/compare/v0.0.1...v1.0.0) (2026-10-27)


### ⚠ BREAKING CHANGES

* **drivers:** SlcanAdapter::Connect returning true no longer means connected. OnConnectionChanged(true) follows once the device has acknowledged the bitrate and the open command; until then IsConnected() is false and sends fail. Constructing an SlcanAdapter (and so a PlatformCanAdapter) needs the timer service.
* **core:** CanFrameTransport::SetOnSendNotification and ClearOnSendNotification are removed. Pass the notification to the three-argument CanFrameTransport constructor instead.
* **server,client:** CanProtocolServerObserver::Online() is notified once when the client comes online, not on every heartbeat. CanProtocolClient::DiscoverCategories returns bool and its completion takes std::optional<infra::ConstByteRange>; std::nullopt means the server did not answer in time. CanSystemCategoryClientObserver is removed.
* **firmware-upgrade:** FirmwareUpgradeCategoryServerObserver::OnDataBlock takes infra::ConstByteRange instead of const hal::Can::Message&; copy the bytes before returning. A Begin Upgrade the application refuses no longer starts the session timer.
* **transport:** Attaching an ISO-TP transport no longer installs its abort callback and no channel is released on failure. Install the callback with IsoTpTransport::SetOnAbort to be told about failed transfers. Sending to a registered peer uses the reversed pair: SendPdu(fcId, dataId) for a channel registered as RegisterReceiveChannel(dataId, fcId). IsoTpChannel::DataId/FcId become RxId/TxId, and Configure takes the receive identifier first.
* **client:** CanSequenceSource replaces PeekSequence and CommitSequence with ReserveSequence, CancelReservation and ReturnSequence. Client categories that send through SendCommand need no change. Code that called PeekSequence or CommitSequence on CanProtocolClient directly, or implements CanSequenceSource, must move to the new operations.
* **drivers:** CanBusAdapter::IsDriverAvailable() is now EnumerateMissingDrivers(callback); CanableAdapter is replaced by SlcanAdapter over a SerialPort; the CAN_LITE_DRIVER_PCAN, _KVASER and _CANABLE options are removed because every driver now builds without an SDK.
* RegisterCategory() return type changes from void to bool on both CanProtocolServer and CanProtocolClient. CanSequenceSource:: CommitSequence() gains category and messageType parameters. CanProtocolClientObserver gains a new pure virtual, OnCommandAckTimeout.
* address repository audit findings (42 issues, #45) ([#46](https://github.com/embedded-pro/can-lite/issues/46))
* CanCategoryServer and CanCategoryClient gained required constructor arguments (CanFrameTransport&, and CanSequenceSource& for clients). FirmwareUpgradeCategoryClient takes CanSequenceSource& instead of CanProtocolClient&. The can_lite.categories.foc_motor target is renamed to can_lite.examples.foc_motor and its headers moved from can-lite/categories/foc_motor/ to examples/foc_motor/.

### Features

* Add drivers ([71a2f2e](https://github.com/embedded-pro/can-lite/commit/71a2f2e147fe2d886cf6a63380e78e094139af6a))
* Add firmware upgrade ([1e5b15f](https://github.com/embedded-pro/can-lite/commit/1e5b15fd1db74feadb109cb79fc5d9f69f7c0b37))
* Add integration tests ([bdd31a6](https://github.com/embedded-pro/can-lite/commit/bdd31a6e076657392c2619f37c6fee0d3ad645b4))
* Add per-layer tracing decorators ([#48](https://github.com/embedded-pro/can-lite/issues/48)) ([58c131f](https://github.com/embedded-pro/can-lite/commit/58c131f363b4a058b69b117c86ac0646eb741e31))
* Can iso tp ([#18](https://github.com/embedded-pro/can-lite/issues/18)) ([79eb4a6](https://github.com/embedded-pro/can-lite/commit/79eb4a6f515b8bb6734818a8397d39e69d460649))
* **drivers:** Add slcan, candleLight and a multi-adapter router; load PCAN and Kvaser at run time ([#64](https://github.com/embedded-pro/can-lite/issues/64)) ([e48ac8a](https://github.com/embedded-pro/can-lite/commit/e48ac8abb8b69511a33e50b73b9934a0142eb2f3))
* Make protocol fully async ([#30](https://github.com/embedded-pro/can-lite/issues/30)) ([5edb028](https://github.com/embedded-pro/can-lite/commit/5edb02838f5ae5f5716e8118d357bd2f9f640379))


### Bug Fixes

* Address repository audit findings (42 issues, [#45](https://github.com/embedded-pro/can-lite/issues/45)) ([#46](https://github.com/embedded-pro/can-lite/issues/46)) ([24aa927](https://github.com/embedded-pro/can-lite/commit/24aa9273bee16bd38ecc71c61f7339a70e6e1e6d))
* Category discovery and sequence resync ([#47](https://github.com/embedded-pro/can-lite/issues/47)) ([85c105a](https://github.com/embedded-pro/can-lite/commit/85c105a9e85c8d35d7297cf4ff79de42dead236d))
* **client:** Consume a command's sequence number only once it is sent ([#67](https://github.com/embedded-pro/can-lite/issues/67)) ([2569ffc](https://github.com/embedded-pro/can-lite/commit/2569ffc3db069f7e31db6bd1b9d15571c73715d3))
* Correct ISO-TP flow-control and N_Bs handling per ISO 15765-2 ([#54](https://github.com/embedded-pro/can-lite/issues/54)) ([02c6b61](https://github.com/embedded-pro/can-lite/commit/02c6b617efaf9bda9cb18b5dd47acdc7de09e5db))
* **drivers:** SocketCAN over netlink, an slcan handshake, whole serial writes, and Windows poll handles ([#77](https://github.com/embedded-pro/can-lite/issues/77)) ([ab9351a](https://github.com/embedded-pro/can-lite/commit/ab9351aff20df0d544e423de0b03d654626e2164))
* **firmware-upgrade:** Open sessions on acceptance and answer without an application ([#72](https://github.com/embedded-pro/can-lite/issues/72)) ([d052cc0](https://github.com/embedded-pro/can-lite/commit/d052cc05d6b90425d3fc102540bf708f498f13e5))
* Prioritise the send queue and reserve capacity for emergency frames ([#60](https://github.com/embedded-pro/can-lite/issues/60)) ([ea80b3e](https://github.com/embedded-pro/can-lite/commit/ea80b3e817efcbd73120962b8a63f243007e4956))
* **server,client:** Report the client online once per transition and tie discovery to the node asked ([#73](https://github.com/embedded-pro/can-lite/issues/73)) ([3fe1c43](https://github.com/embedded-pro/can-lite/commit/3fe1c43be7daab9ad6b9218ac0860780ecab855a))
* **transport:** Follow ISO 15765-2 on unexpected frames and keep channels across failures ([#68](https://github.com/embedded-pro/can-lite/issues/68)) ([e710398](https://github.com/embedded-pro/can-lite/commit/e710398dd0b88cdad3b0c05d69ac93409de1ae8a))
* **transport:** ISO 15765-2 consecutive-frame length, N_As and receiver-chosen pacing ([#71](https://github.com/embedded-pro/can-lite/issues/71)) ([a38d95a](https://github.com/embedded-pro/can-lite/commit/a38d95a48f19e2dff86cf368da56b9c2f238d6db))


### Code Refactoring

* **core:** Payload and API hygiene ([#75](https://github.com/embedded-pro/can-lite/issues/75)) ([a2abf89](https://github.com/embedded-pro/can-lite/commit/a2abf89f845975d30c7e09327541a17e4deb512f))
* Make the category API generic and move FOC motor to examples ([#44](https://github.com/embedded-pro/can-lite/issues/44)) ([52eebf9](https://github.com/embedded-pro/can-lite/commit/52eebf97581b0a49778307d6d267019112837852))
