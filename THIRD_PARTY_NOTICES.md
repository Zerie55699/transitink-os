# Third-party notices

TransitInk OS source code is licensed under Apache-2.0. This repository also
contains or links third-party components with separate licences and terms.

## Noto Sans CJK HK Regular

TransitInk OS vendors the unmodified Noto Sans CJK HK Regular 2.004 font under
`third_party/fonts/noto-sans-cjk-hk/`. Its source provenance and SHA-256 digest
are recorded in `SOURCE.md`; the complete SIL Open Font License Version 1.1 is
retained in `OFL.txt`, and the upstream copyright and trademark statement is
retained in `UPSTREAM-NOTICE.md`.

The generated 16-pixel bitmap glyph table in
`src/generated/HkGlyphFontData.cpp` is derived from that font and is distributed
under the same OFL-1.1 terms.

## yxml

The vendored yxml source under `lib/yxml/` retains its upstream copyright,
licence, version, and source hashes in `lib/yxml/LICENSE`.

## Build dependencies

PlatformIO downloads the remaining framework and library dependencies declared
by `platformio.ini`. Their own upstream licences continue to apply. Release
packages should include the notices required by the exact dependency versions
resolved for that release.
