# Changelog

## Unreleased

### Added

- Windows-triggered FicTrac fork update package refreshed from the validated MultiBiOS vendor tree.

### Changed

- Spinnaker first-frame wait is configurable through `src_first_frame_timeout_ms` instead of relying on the upstream fixed wait behavior.
- Windows shutdown path supports graceful termination through `SIGBREAK` so `CTRL_BREAK_EVENT` can unwind camera state cleanly.
- Trigger-stream end on Spinnaker `NEW_BUFFER_DATA` / `-1011` is treated as normal end-of-stream rather than a fatal failure.
- Raw frame recording is chunked for reliable post-run reconstruction instead of depending on a single final AVI flush.

### Fixed

- Spinnaker 4.x image conversion compatibility in the USB3 camera path.
- null-image cleanup during failed frame grabs.

## Initial Publication Note

This changelog should remain focused on native FicTrac fork behavior. Downstream MultiBiOS integration changes should be documented in the consumer repository, not duplicated here except where they justify a native fork change.
