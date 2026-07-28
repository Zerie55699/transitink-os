# Changelog

All notable user-visible changes to TransitInk OS will be documented here.

## 1.1.3 - 2026-07-28

- Added a display-font setting with Noto Sans as the default and GNU Unifont as
  an optional native bitmap font.
- Bundled the pinned GNU Unifont source, licence, provenance, and deterministic
  generated glyph subset in release packages.

## 1.1.2 - 2026-07-28

- Fixed a blank e-paper display after installing TransitInk OS over firmware
  that left the Zectrix Note 4 display-power GPIO in a held low state.

## 1.1.1 - 2026-07-27

- Added an on-device firmware update path that downloads a verified release
  image, installs it to the inactive application partition, and preserves
  Wi-Fi, widgets, routes, and other saved settings.
- Added release packaging for both complete browser installation images and
  settings-preserving OTA images.
- Separated the public installer into clear full-install and
  settings-preserving update paths, with the data impact shown before users
  choose a method.

## 1.1.0 - 2026-07-26

- Expanded the dashboard from four widgets to three pages of four widgets;
  Volume Down cycles through configured pages while preserving power-efficient
  refresh behaviour.
- Added embedded London bus and rail catalogues, live TfL arrivals, United
  Kingdom weather locations, and London time-zone support.
- Added Traditional Chinese and English interfaces, bilingual transport and
  weather labels, and more compact English e-paper layouts.
- Embedded complete Hong Kong and London transport catalogues for offline route,
  direction, and stop selection, with manual on-device catalogue updates.
- Distinguished loading, no-service, expired, and unavailable states so page
  changes and failed live requests no longer show misleading arrival status or
  hide fixed route and stop labels.
- Improved settings-page mobile layout, first-time setup guidance, save/restart
  safety, wake-button responsiveness, and scheduled/background wake handling.

## 1.0.2 - 2026-07-23

- Added an optional daily automatic wake window, including schedules which
  cross midnight.
- Kept normal button wake behaviour outside the configured automatic window.
- Avoided periodic sleeping-maintenance wake-ups while the daily schedule is
  enabled, reducing unnecessary battery use.

## 1.0.1 - 2026-07-22

- Fixed the browser installer image so it preserves the Zectrix Note 4
  bootloader flash mode and boots after installation.
- Added a release-time check that rejects installer firmware which differs from
  PlatformIO's canonical factory image.
- Changed future TransitInk OS releases to the PolyForm Noncommercial License
  1.0.0; separately licensed third-party components retain their own terms.
- Added an on-device prompt directing users to press Volume Up when no dashboard
  widgets have been configured.
