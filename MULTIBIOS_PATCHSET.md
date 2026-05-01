# MultiBiOS FicTrac Patchset

This vendor tree is not a disposable upstream checkout. It contains the rig-specific FicTrac changes that are currently required for MultiBiOS on Windows with Spinnaker-triggered Blackfly cameras.

Why this file exists:

- it gives this repo a stable, source-adjacent patch inventory
- it identifies which edits belong to the native FicTrac fork versus the MultiBiOS Python wrapper
- it is the minimum publication manifest needed to seed a standalone lab-owned fork later

## Scope

This document only covers changes that live in or directly define the maintained native FicTrac fork.

MultiBiOS-side integration changes remain documented in:

- `docs/fictrac.md`
- `multibios/fictrac_client.py`
- `multibios/run_protocol.py`
- `multibios/fictrac_raw_recording.py`

## Native Patch Inventory

### 1. Spinnaker 4.x image conversion compatibility

Files:

- `src/PGRSource.cpp`

Purpose:

- update the Spinnaker conversion path to a form that works with the installed Spinnaker 4.x SDK on this workstation

### 2. Configurable first-frame wait for triggered startup

Files:

- `include/PGRSource.h`
- `src/PGRSource.cpp`
- `src/Trackball.cpp`
- `src/ConfigGUI.cpp`

Purpose:

- replace the upstream fixed first-frame wait with a configurable `src_first_frame_timeout_ms`
- preserve the rig-safe default of infinite first-frame wait when the value is `0` or negative
- ensure the classic FicTrac configuration UI preserves that behavior when re-run on this rig

### 3. Safe exception cleanup around failed frame grabs

Files:

- `src/PGRSource.cpp`

Purpose:

- prevent null-image cleanup from masking the original capture failure

### 4. Graceful Windows termination and clean trigger-stream drain

Files:

- `exec/fictrac.cpp`
- `include/PGRSource.h`
- `src/PGRSource.cpp`
- `src/FrameGrabber.cpp`

Purpose:

- accept `SIGBREAK` on Windows so `CTRL_BREAK_EVENT` can unwind FicTrac cleanly
- treat Spinnaker `NEW_BUFFER_DATA` / `-1011` at end-of-trigger as normal stream completion instead of a fatal error
- ensure the Spinnaker camera reaches `EndAcquisition()` and `DeInit()` before exit

### 5. Chunked native raw recording for reliable post-run recovery

Files:

- `include/FileRecorder.h`
- `src/FileRecorder.cpp`
- `src/Recorder.cpp`
- `src/FrameGrabber.cpp`

Purpose:

- write raw frame chunks and frame indices incrementally instead of relying on one final AVI flush during shutdown
- keep saved-frame accounting aligned to FicTrac `log_frame`

## Validated Behavior On This Rig

The currently validated packaged binary is:

- `C:/Rishika/MultiBiOS/assets/fictrac-spinnaker/fictrac-spinnaker.exe`

Validated operational requirements:

- hardware-triggered Blackfly camera via Spinnaker
- Windows graceful stop through `CTRL_BREAK_EVENT`
- canonical rig config at `config/hardware.yaml` + `config/config_camera.txt`
- display-on reliable repeatability point: `7.0 ms` camera interval (`142.857143 Hz`)

## Publication Boundary

If this patch set is published as a standalone lab repository, publish this native tree as the fork boundary and keep MultiBiOS integration in a separate consumer repository.

Recommended standalone repo contents:

- the full FicTrac source tree with upstream license preserved
- this patch manifest
- a short changelog describing divergence from upstream
- build instructions for the validated Windows Spinnaker toolchain
- tags that map packaged binaries to exact source states

The local publication scaffold prepared in this workspace is:

- `Fictrac-TrigWin/`

That scaffold names the planned standalone repository explicitly as:

- `Fictrac-TrigWin`

Do not publish only the packaged executable without the corresponding source state.
