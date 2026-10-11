# Evohome Dashboard for Home Assistant

Independent, HACS-installable Home Assistant integration and accessible dashboard for Honeywell Home / Resideo Evohome (European TCC systems).

**Status:** Phase 1 under development. Not yet validated on a live controller.

The project does **not** incorporate code from `colegates/evohome-control`. It uses the Apache-2.0 licensed `evohome-async` transport library used by Home Assistant's upstream Evohome integration.

See `docs/ARCHITECTURE.md` and `docs/FEATURE_MATRIX.md`. The first release prioritizes everyday touchscreen replacement: heating zone controls, modes, schedules, hot water, user-friendly UI, diagnostics and a safe advanced-settings catalog. Installer-only device configuration cannot be remotely exposed until verified against supported APIs.

## Security
Do not paste or commit Evohome passwords, refresh tokens, traffic dumps with credentials, or Home Assistant access tokens.
