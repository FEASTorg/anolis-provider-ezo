# Phase 07 Mixed-Bus Validation

This phase now uses a minimal two-profile flow:

1. Windows mock validation.
2. Linux real-hardware validation.

Canonical inputs:

1. Config pack: [`config/phase7/`](../../config/phase7/README.md)
2. Commands: [`config/phase7/COMMANDS.md`](../../config/phase7/COMMANDS.md)

Pass criteria:

1. Windows mock profile starts and serves runtime endpoints.
2. Linux hardware profile starts and `check_mixed_bus_http.sh` exits `0`.
