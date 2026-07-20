# Security policy

## Supported versions

Security fixes are provided for the latest published TransitInk OS release.
Older firmware versions may not receive backports.

## Reporting a vulnerability

Please use GitHub private vulnerability reporting for security issues. Do not
open a public issue containing credentials, device backups, Wi-Fi configuration,
private portal tokens, or reproduction details that would put deployed devices
at risk.

Include the affected firmware version, hardware profile, impact, reproduction
steps and any proposed mitigation. Maintainers will acknowledge a complete
report through the private advisory and coordinate disclosure after a fix is
available.

## Security boundaries

- First-time setup uses a temporary access point with a new random 12-character
  WPA2 password shown on the e-paper display. Once normal Wi-Fi is connected,
  pressing the physical Volume button opens the portal on the device's LAN IP
  instead. The portal remains available until settings are saved, the device is
  restarted, or Volume is pressed again. It is not served continuously during
  normal dashboard use.
- LAN portal URLs contain a random per-session capability path in the on-device
  QR code. Portal API requests also require the expected local interface and
  host, the capability token, same-origin mutating requests, and the per-session
  CSRF token.
- Official transit, journey-time, and weather HTTPS requests verify the server
  hostname and certificate chain against the pinned Hongkong Post Root CA 3.
- The web installer serves its fixed ESP Web Tools copy from the same origin,
  applies a restrictive Content Security Policy, and release workflows publish
  SHA-256 checksums plus GitHub build-provenance attestations.

## Local secrets and physical access

Wi-Fi credentials are stored in ESP32 NVS so the device can reconnect after a
restart. General browser-installable builds do not enable ESP32 flash encryption
or Secure Boot because those eFuse settings are device-owner-specific and can be
irreversible. An attacker with physical flash access may therefore recover
device settings. Use a separate IoT network, keep the device physically secure,
and factory-reset it before transfer or disposal.

Full-flash backups contain the same device secrets. `scripts/backup_flash.sh`
creates them with owner-only permissions, but users remain responsible for
secure storage and deletion.

## Build-tool audit exception

PlatformIO 6.1.19 requires Starlette below 0.53 for its optional local Home web
interface, while published fixes for the current Starlette advisories require
1.x. TransitInk CI and contributor commands do not start PlatformIO Home;
`scripts/audit_python_tools.sh` records only those five specific exceptions and
continues to fail on every other known Python package vulnerability. Remove the
exceptions when PlatformIO supports a fixed Starlette release.
