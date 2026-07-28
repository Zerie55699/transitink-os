# Third-party notices

Original TransitInk OS source code is licensed under the PolyForm
Noncommercial License 1.0.0. This repository also contains or links third-party
components with separate licences and terms; the project licence does not
replace or restrict those third-party terms.

## Noto Sans CJK HK Regular

TransitInk OS vendors the unmodified Noto Sans CJK HK Regular 2.004 font under
`third_party/fonts/noto-sans-cjk-hk/`. Its source provenance and SHA-256 digest
are recorded in `SOURCE.md`; the complete SIL Open Font License Version 1.1 is
retained in `OFL.txt`, and the upstream copyright and trademark statement is
retained in `UPSTREAM-NOTICE.md`.

The generated 16-pixel bitmap glyph table in
`src/generated/HkGlyphFontData.cpp` is derived from that font and is distributed
under the same OFL-1.1 terms.

## GNU Unifont

TransitInk OS vendors the GNU Unifont 17.0.04 compiled BDF under
`third_party/fonts/unifont/`. Its source URL, upstream copyright statement and
SHA-256 digest are recorded in `SOURCE.md`, and the complete SIL Open Font
License Version 1.1 is retained in `OFL-1.1.txt`.

The selectable pixel-font table in
`src/generated/UnifontGlyphFontData.cpp` contains an un-antialiased subset of
the original 8×16 and 16×16 bitmap glyphs and remains under OFL-1.1.

## yxml

The vendored yxml source under `lib/yxml/` retains its upstream copyright,
licence, version, and source hashes in `lib/yxml/LICENSE`.

## ESP Web Tools

The browser distribution of ESP Web Tools 10.2.1 is vendored under
`installer/esp-web-tools/` so the installer does not execute CDN-hosted code.
Its npm package digest and local Traditional Chinese dialog patch are recorded
in `installer/esp-web-tools/README.md`; the upstream Apache-2.0 licence is
retained in `installer/esp-web-tools/LICENSE`. Components compiled into the
browser bundle, including Lit, Material Web, esptool-js, pako, tslib,
atob-lite, and Improv Wi-Fi Serial SDK, are listed in
`installer/esp-web-tools/THIRD_PARTY_NOTICES.md`; their licence texts are kept
under `third_party/licenses/`.

`installer/esp-web-tools/vendor/install-button.js` and
`installer/esp-web-tools/no-port-dialog-zh.js` carry prominent modification
notices. TransitInk changes only the no-port dialog import, its Traditional
Chinese copy, and same-origin module paths.

## Firmware framework and libraries

The released firmware is linked with the following pinned components:

| Component | Version | Licence | Licence file |
| --- | --- | --- | --- |
| Arduino core for ESP32 | 3.3.9 | LGPL-2.1-or-later | `third_party/licenses/LGPL-2.1.txt` |
| ArduinoJson | 6.21.6 | MIT | `third_party/licenses/ArduinoJson-MIT.txt` |
| Adafruit GFX Library | 1.12.6 | BSD-3-Clause | `third_party/licenses/Adafruit-GFX-BSD-3-Clause.txt` |
| Adafruit BusIO | 1.17.4 | MIT | `third_party/licenses/Adafruit-BusIO-MIT.txt` |
| QRCode by Richard Moore | 0.0.1 | MIT | `third_party/licenses/QRCode-MIT.txt` |

The Arduino core is a separately licensed library. TransitInk OS does not
relicense it under the project licence. Exact framework source,
precompiled-library provenance, build instructions, and the route for relinking
a modified core are recorded in `CORRESPONDING_SOURCE.md` and are included with
binary releases.

## Hardware implementation references

The initial Zectrix Note 4 board mapping and SSD1683 handling were informed by
[`mac20777/vibecoding-voice`](https://github.com/mac20777/vibecoding-voice),
licensed under MIT (Copyright (c) 2026 macheng2017). The retained licence is
`third_party/licenses/VibecodingVoice-MIT.txt`.

The e-paper partial-refresh policy was informed by
[`qiujun8023/slate`](https://github.com/qiujun8023/slate), licensed under MIT
(Copyright (c) 2026). The retained licence is
`third_party/licenses/Slate-MIT.txt`.

TransitInk's board profile, display driver and refresh scheduler are maintained
locally and may differ from both references.

## Product image

The Zectrix Note 4 product visual used in the README and installer is an
AI-assisted image based on a device photograph supplied by the project
maintainer. Its provenance and digest are recorded in
`installer/assets/SOURCE.md`. TransitInk OS claims no rights in the Zectrix
name, trademarks, or product design beyond descriptive use.

## Trademarks and non-endorsement

TransitInk OS is an independent source-available project and is not affiliated
with or endorsed by Zectrix. TransitInk OS 是獨立原始碼公開專案，與 Zectrix 沒有從屬或認可關係。
Zectrix and all other third-party names and marks are
the property of their respective owners. Their use identifies compatible
hardware or data sources and does not imply sponsorship or endorsement.

## Release distribution

`scripts/package_installer.py` copies the project licence, these notices, the
transport-data attribution, corresponding-source instructions, font notices,
and all retained third-party licence texts into `legal/`. The same material is
published on the installer site and inside each downloadable firmware bundle.
