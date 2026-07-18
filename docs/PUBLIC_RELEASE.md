# Creating the public repository

TransitInk OS should be published as a new repository from a reviewed snapshot,
not by making the existing private development history public. Earlier commits
contain generated glyph output from before the redistributable Noto font
pipeline was established, while the current tree contains the complete source,
licence provenance and deterministic generators.

## Before creating the repository

1. Confirm the canonical Apache License 2.0 text remains in `LICENSE`. The font,
   generated glyphs, yxml and transport data retain the separate terms
   documented in `THIRD_PARTY_NOTICES.md` and `THIRD_PARTY_DATA.md`.
2. Complete every required item in `OPEN_SOURCE_CHECKLIST.md`.
3. Run the complete test suite, clean firmware build and installer packager from
   a fresh checkout.
4. Run a dedicated secret scanner over both the reviewed snapshot and any Git
   history that will actually be pushed. CI repeats this with Gitleaks; local
   release preparation should run `gitleaks dir .` and `gitleaks git .`.
5. Flash the release candidate to the supported Zectrix Note 4 revision and
   record the result in the release notes.

## Repository settings

- Use `main` as the default protected branch and require the CI workflow.
- Set GitHub Pages source to **GitHub Actions**.
- Enable private vulnerability reporting before accepting public reports.
- Keep Actions permissions at their workflow defaults; the release workflow
  requests write access only for tagged releases and Pages deployment.

## First release

Confirm that `FIRMWARE_VERSION` is the intended semantic version, commit the
reviewed snapshot, and create a matching `vX.Y.Z` tag. The tag workflow will:

1. verify the glyph and transport catalog generators;
2. run all host, native and structure tests;
3. build the Zectrix Note 4 firmware;
4. reject a tag/version mismatch;
5. create one merged ESP32-S3 image and SHA-256 checksum;
6. publish the image as a GitHub Release asset; and
7. deploy the same image and manifest as the Pages web installer.

After the new Pages URL is confirmed, update the former
`wongshingg/zectrix-note4-installer` site to redirect users to it, then archive
that repository. Do not maintain two writable installer release pipelines.
