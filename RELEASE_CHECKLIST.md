# Release Checklist

Use this checklist before updating the lab-owned `Fictrac-TrigWin` repository and before each subsequent release.

## Initial Publication

- copy the full native source tree from `assets/third_party/FicTrac`
- preserve upstream `LICENSE.txt`
- include `MULTIBIOS_PATCHSET.md`
- include a fork-aware `README.md`
- include `CHANGELOG.md`
- include this `RELEASE_CHECKLIST.md`
- verify no build outputs or packaged binaries are committed accidentally
- verify no MultiBiOS-only Python or DAQ code is included
- record the upstream base commit or release if known
- verify the destination remote and default branch before pushing updates

## Pre-Release Validation

- confirm the native tree still builds with the validated Windows Spinnaker toolchain
- confirm the packaged executable still launches under MultiBiOS on the target workstation
- confirm first-frame wait behavior remains correct for `src_first_frame_timeout_ms = 0`
- confirm graceful stop still exits cleanly on Windows
- confirm chunked raw recording still reconstructs cleanly in downstream postprocess

## Release Metadata

- create a tag that maps to the published native source state
- record the corresponding packaged executable provenance in release notes
- summarize user-visible native changes in `CHANGELOG.md`
- keep downstream MultiBiOS release notes separate from native FicTrac release notes
