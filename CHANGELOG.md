# Changelog

## Unreleased

### Added

- Windows-triggered FicTrac fork update package refreshed from the validated MultiBiOS vendor tree.
- Native sample-video parity regression test for the bundled offline sample input.
- Native recorder shutdown regression test covering queued-write drain behavior during teardown.
- Native parser and runtime config-validation regression tests covering malformed lines, duplicate keys, strict scalar/vector parsing, and semantic validation of present config values.
- Native `--validate` and `--preflight` CLI modes for config-only and startup-without-tracking checks before a real run.

### Changed

- Spinnaker first-frame wait is configurable through `src_first_frame_timeout_ms` instead of relying on the upstream fixed wait behavior.
- Windows shutdown path supports graceful termination through `SIGBREAK` so `CTRL_BREAK_EVENT` can unwind camera state cleanly.
- Trigger-stream end on Spinnaker `NEW_BUFFER_DATA` / `-1011` is treated as normal end-of-stream rather than a fatal failure.
- Raw frame recording is chunked for reliable post-run reconstruction instead of depending on a single final AVI flush.
- Display-only sphere-view rendering now happens in the draw path instead of cloning display buffers from the tracking loop.
- Debug-path history maintenance and display-frame handoff now stay in the draw path so the tracking loop no longer clones source and ROI frames just to feed the visualization canvas.
- ConfigGUI prompts and method selection now run inside the OpenCV window instead of splitting interaction between the GUI and terminal.
- ConfigGUI live-camera selection can be done from an in-window chooser before opening a numeric PGR source.
- Runtime startup now validates structured ROI/C2A config keys and ignored-path option values up front, so malformed present values fail even when a fallback path would otherwise mask them.

### Fixed

- Spinnaker 4.x image conversion compatibility in the USB3 camera path.
- Async recorder teardown now drains queued messages before exit so final log, data, socket, serial, and sidecar writes are not dropped during shutdown.
- null-image cleanup during failed frame grabs.
- ConfigGUI keep/reconfigure prompts no longer freeze the window while waiting for terminal-style input.
- ConfigGUI compact footer/help overlays wrap correctly and avoid the previous oversized instruction panel behavior.
- ConfigGUI user cancellation now exits cleanly instead of being reported as a runtime failure.
- Config files with malformed lines, duplicate keys, trailing junk scalars, or malformed vectors now fail parsing instead of being partially accepted.
- FicTrac startup no longer rewrites runtime config files just to inject defaults during ordinary tracking startup.
- Present-but-invalid runtime config values now fail fast across scalar, vector, and enumerated keys instead of silently falling back to defaults or alternate paths.
- Native FicTrac now exits non-zero when startup or runtime failure occurs, so downstream launchers can distinguish real failures from clean shutdown.

## Initial Publication Note

This changelog should remain focused on native FicTrac fork behavior. Downstream MultiBiOS integration changes should be documented in the consumer repository, not duplicated here except where they justify a native fork change.
