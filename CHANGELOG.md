# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
- Added English and Traditional Chinese documentation.

### Changed

- Changed stream metrics to use sliding-window statistics.
- Changed the project license to MIT.
