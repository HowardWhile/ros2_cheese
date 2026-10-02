# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Added Docker-based Debian packaging for x86_64 and arm64, including ZIP
  archives containing the `cheese` and `cheese_interfaces` packages.
- Added a release installation script with automatic architecture detection
  and support for online downloads and offline installation.
- Added Debian packaging and installation documentation.
- Added stream recovery integration tests covering source switching,
  disconnections, invalid images, stale captures, paused ROS time, and
  injected subscription creation failures.

### Changed

- Increased the image stream timeout from two to three seconds.
- Changed stream timing and statistics to use a steady clock so paused ROS
  time does not prevent timeout detection or recovery.
- Added green and red highlighting to stream health logs.

### Fixed

- Detect image sources from publisher endpoints instead of topic listings
  that may include the node's own subscriptions.
- Recover subscriptions when publishers disappear or return, retry failed
  subscription creation, and switch between raw and compressed sources when
  the selected source times out and an alternative is available.
- Preserve a healthy source selection when other publishers appear or
  disappear, and clear cached images and statistics when switching sources.
- Ignore queued messages from retired subscriptions and reject captures
  until a valid image arrives from the new source or when the stream times out.
- Handle empty, oversized, and invalid compressed image data without letting
  decoding exceptions terminate the node.
- Verify saved captures exist, are non-empty, can be decoded, and have the
  expected dimensions, with up to five verification attempts spaced 100 ms
  apart before reporting failure.

## [0.1.1] - 2026-07-03

### Added

- Added English and Traditional Chinese documentation.
- Added dedicated status and parameter reference documentation.
- Added package-level changelogs for the ROS release workflow.

### Changed

- Changed the project license to MIT.
- Replaced the vendored nlohmann JSON header with a system dependency.
- Completed package metadata, dependencies, and lint configuration for ROS 2
  Jazzy packaging.

## [0.0.1] - 2026-06-16

### Added

- Added the `cheese` image capture node with support for raw and compressed
  image topics.
- Added standard trigger and custom string trigger services for capturing
  images on demand.
- Added optional filename suffixes through
  `cheese_interfaces/srv/StringTrigger`.
- Added image stream health reporting, including FPS, bandwidth, processing
  failures, and capture directory statistics.
- Added configurable file count and storage limits with automatic removal of
  older captures.
- Added launch arguments for the image topic, output directory, node name,
  namespace, file limit, and storage limit.

### Changed

- Changed stream metrics to use sliding-window statistics.

[Unreleased]: https://github.com/HowardWhile/ros2_cheese/compare/05d2e728148281289bb51ecbc21fdc1666c5138d...HEAD
[0.1.1]: https://github.com/HowardWhile/ros2_cheese/compare/8ef7a986c2b4ddf9a3fbfd49445db70ebcb0596e...05d2e728148281289bb51ecbc21fdc1666c5138d
[0.0.1]: https://github.com/HowardWhile/ros2_cheese/tree/8ef7a986c2b4ddf9a3fbfd49445db70ebcb0596e
