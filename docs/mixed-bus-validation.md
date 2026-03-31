# Mixed-Bus Validation

This repository uses a minimal profile flow:

1. Windows mock validation.
2. Linux real-hardware baseline validation.
3. Linux real-hardware lab validation.

Canonical inputs:

1. Config pack: [`config/mixed-bus/`](../config/mixed-bus/README.md)
2. Commands: [`config/mixed-bus/COMMANDS.md`](../config/mixed-bus/COMMANDS.md) (includes preset-based build prerequisites)

Linux hardware profiles require:
1. `anolis-provider-bread` built with `dev-linux-hardware-release`.
2. `anolis-provider-ezo` built with `dev-linux-hardware-release`.
The bread provider configs enforce this via `hardware.require_live_session: true` (fail-fast startup guard).
In `anolis-provider-ezo`, `dev-linux-hardware-*` presets are cross-provider naming aliases.

Pass criteria:

1. Windows mock profile starts and serves runtime endpoints.
2. Linux baseline profile starts and `check_mixed_bus_http.sh` exits `0` with 5-device inventory.
3. Linux lab profile starts and `check_mixed_bus_http.sh` exits `0` with expected lab inventory.
4. Linux lab expected inventory aligns with CRUMBS lab validation map: `rlht0@0x0A`, `dcmt0@0x14`, `dcmt1@0x15`, `ph0@0x63`, `do0@0x61`.
5. Bosch optional validation from CRUMBS (`0x76`/`0x77`) remains outside provider mixed-bus scope.
