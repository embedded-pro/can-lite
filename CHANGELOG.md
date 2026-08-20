# Changelog

## [1.0.0](https://github.com/embedded-pro/can-lite/compare/v0.0.1...v1.0.0) (2026-08-20)


### ⚠ BREAKING CHANGES

* RegisterCategory() return type changes from void to bool on both CanProtocolServer and CanProtocolClient. CanSequenceSource:: CommitSequence() gains category and messageType parameters. CanProtocolClientObserver gains a new pure virtual, OnCommandAckTimeout.
* address repository audit findings (42 issues, #45) ([#46](https://github.com/embedded-pro/can-lite/issues/46))
* CanCategoryServer and CanCategoryClient gained required constructor arguments (CanFrameTransport&, and CanSequenceSource& for clients). FirmwareUpgradeCategoryClient takes CanSequenceSource& instead of CanProtocolClient&. The can_lite.categories.foc_motor target is renamed to can_lite.examples.foc_motor and its headers moved from can-lite/categories/foc_motor/ to examples/foc_motor/.

### Features

* Add drivers ([71a2f2e](https://github.com/embedded-pro/can-lite/commit/71a2f2e147fe2d886cf6a63380e78e094139af6a))
* Add firmware upgrade ([1e5b15f](https://github.com/embedded-pro/can-lite/commit/1e5b15fd1db74feadb109cb79fc5d9f69f7c0b37))
* Add integration tests ([bdd31a6](https://github.com/embedded-pro/can-lite/commit/bdd31a6e076657392c2619f37c6fee0d3ad645b4))
* Add per-layer tracing decorators ([#48](https://github.com/embedded-pro/can-lite/issues/48)) ([58c131f](https://github.com/embedded-pro/can-lite/commit/58c131f363b4a058b69b117c86ac0646eb741e31))
* Can iso tp ([#18](https://github.com/embedded-pro/can-lite/issues/18)) ([79eb4a6](https://github.com/embedded-pro/can-lite/commit/79eb4a6f515b8bb6734818a8397d39e69d460649))
* Make protocol fully async ([#30](https://github.com/embedded-pro/can-lite/issues/30)) ([5edb028](https://github.com/embedded-pro/can-lite/commit/5edb02838f5ae5f5716e8118d357bd2f9f640379))


### Bug Fixes

* Address repository audit findings (42 issues, [#45](https://github.com/embedded-pro/can-lite/issues/45)) ([#46](https://github.com/embedded-pro/can-lite/issues/46)) ([24aa927](https://github.com/embedded-pro/can-lite/commit/24aa9273bee16bd38ecc71c61f7339a70e6e1e6d))
* Category discovery and sequence resync ([#47](https://github.com/embedded-pro/can-lite/issues/47)) ([85c105a](https://github.com/embedded-pro/can-lite/commit/85c105a9e85c8d35d7297cf4ff79de42dead236d))


### Code Refactoring

* Make the category API generic and move FOC motor to examples ([#44](https://github.com/embedded-pro/can-lite/issues/44)) ([52eebf9](https://github.com/embedded-pro/can-lite/commit/52eebf97581b0a49778307d6d267019112837852))
