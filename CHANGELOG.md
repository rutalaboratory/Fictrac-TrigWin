# Changelog

## Unreleased

### Added

- Windows-triggered FicTrac fork update package refreshed from the validated MultiBiOS vendor tree.
- Native sample-video parity regression test for the bundled offline sample input.

### Changed

- Spinnaker first-frame wait is configurable through `src_first_frame_timeout_ms` instead of relying on the upstream fixed wait behavior.
- Windows shutdown path supports graceful termination through `SIGBREAK` so `CTRL_BREAK_EVENT` can unwind camera state cleanly.
- Trigger-stream end on Spinnaker `NEW_BUFFER_DATA` / `-1011` is treated as normal end-of-stream rather than a fatal failure.
- Raw frame recording is chunked for reliable post-run reconstruction instead of depending on a single final AVI flush.
- Display-only sphere-view rendering now happens in the draw path instead of cloning display buffers from the tracking loop.
- Debug-path history maintenance and display-frame handoff now stay in the draw path so the tracking loop no longer clones source and ROI frames just to feed the visualization canvas.
- ConfigGUI prompts and method selection now run inside the OpenCV window instead of splitting interaction between the GUI and terminal.
- ConfigGUI live-camera selection can be done from an in-window chooser before opening a numeric PGR source.

### Fixed

- Spinnaker 4.x image conversion compatibility in the USB3 camera path.
- null-image cleanup during failed frame grabs.
- ConfigGUI keep/reconfigure prompts no longer freeze the window while waiting for terminal-style input.
- ConfigGUI compact footer/help overlays wrap correctly and avoid the previous oversized instruction panel behavior.
- ConfigGUI user cancellation now exits cleanly instead of being reported as a runtime failure.

## Initial Publication Note

This changelog should remain focused on native FicTrac fork behavior. Downstream MultiBiOS integration changes should be documented in the consumer repository, not duplicated here except where they justify a native fork change.
