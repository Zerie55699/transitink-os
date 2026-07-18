# Open-source release checklist

The codebase and repository layout are prepared for public collaboration, but
the unchecked legal and release items below must be resolved before calling the
project open source.

## Completed repository preparation

- [x] Document the production and test directory boundaries.
- [x] Ignore build caches, local toolchains, device backups, local design state,
      generated release packages, and the legacy installer clone.
- [x] Pin the verified Python tools and PlatformIO ESP32 platform version.
- [x] Add a CI gate for the complete test suite and firmware build.
- [x] Replace the machine-specific serial port in public usage examples.
- [x] Integrate the web installer source with the firmware repository.
- [x] Generate the merged image, manifest, checksum and release metadata from
      the firmware build without committing release binaries.
- [x] Add a tag-gated workflow that publishes one tested image to both GitHub
      Releases and GitHub Pages.

## Required before public release

- [x] License the project source under Apache-2.0 and retain the canonical text
      in `LICENSE`.
- [x] Regenerate the Traditional Chinese glyph data from pinned Noto Sans CJK HK
      Regular 2.004, retain its OFL-1.1 licence and source SHA-256, and verify the
      generated table in CI.
- [ ] Publish from a clean current tree, or remove pre-Noto generated glyph data
      from the public Git history before exposing the existing private history.
      For the planned new public repository, prefer a fresh initial history from
      the reviewed current tree instead of pushing this private history.
- [ ] Review all vendored and linked dependencies and add any required notices.
      `lib/yxml/LICENSE` is already retained.
- [x] Add a dedicated Gitleaks CI job and a narrow allowlist for ignored local
      tool, build, backup and legacy-repository directories.
- [ ] Review the exact snapshot and complete history that will be pushed with a
      dedicated secret scanner immediately before creating the public remote.
- [x] Confirm that every required firmware source, catalog asset, board file,
      script, partition table, installer source and test is tracked in the
      clean release-preparation baseline.
- [ ] Create the public repository remote and enable GitHub Actions.
- [ ] Enable GitHub private vulnerability reporting when the public repository
      is created; the checked-in `SECURITY.md` directs reports to that channel.
- [ ] Build on a clean checkout, run the full test suite, and flash the release
      candidate to the target device.
- [ ] Publish source, release notes, firmware checksum, installer manifest, and
      firmware version as one coordinated release.

## Recommended follow-up

- [ ] Add issue templates after the public repository URL and support policy are
      final.
- [x] Add a changelog starting with the first public version.
- [ ] Document any hardware revisions tested by contributors.
- [ ] After the new Pages URL is live, replace the former standalone installer
      page with a redirect and archive its repository.
