# Contributing to TransitInk OS

Thanks for helping improve TransitInk OS. Start with
[Project structure](docs/PROJECT_STRUCTURE.md) and
[Development](docs/DEVELOPMENT.md) before changing production code.

## Local checks

Set up the repository-local tools, run the complete test suite, and build the
firmware:

```bash
scripts/install_tools.sh
python3 -m unittest discover -s tests -p "test_*.py" -q
PLATFORMIO_CORE_DIR="$PWD/.platformio" .venv/bin/platformio run -e zectrix_note4
```

Changes that affect board pins, power, sleep/wake behaviour, flash layout, or
e-paper refresh policy should also be verified on the target hardware. Include
the board revision and verification performed in the pull request.

## Pull requests

- Keep each change focused and explain its user-visible or hardware impact.
- Add tests for new behaviour and preserve the existing module boundaries.
- Do not include generated build output, device backups, credentials, local
  access-point tokens, or private serial logs.
- Regenerate generated source with its checked-in script; do not edit generated
  tables by hand.
- Update public documentation when commands, configuration, or supported
  hardware change.

Unless explicitly stated otherwise, contributions intentionally submitted for
inclusion in TransitInk OS must be licensed under the
[PolyForm Noncommercial License 1.0.0](LICENSE). By submitting a contribution,
you confirm that you have the right to provide it under those terms.
Follow the release procedure in [Development](docs/DEVELOPMENT.md) before
publishing a release.
