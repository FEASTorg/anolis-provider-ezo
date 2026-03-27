# anolis-provider-ezo

EZO sensor hardware provider for the Anolis runtime.

## Current status
1. Phase 0 implemented: architecture + contract lock docs in `docs/`.
2. Phase 1 implemented: runnable ADPP provider skeleton with config parsing, framed stdio transport, and baseline unit tests.

## Build
```bash
cmake -S . -B build/dev -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dev
```

## Test
```bash
ctest --test-dir build/dev --output-on-failure
```

## Run
```bash
./build/dev/anolis-provider-ezo --check-config config/example.local.yaml
./build/dev/anolis-provider-ezo --config config/example.local.yaml
```

## Docs
1. [docs/README.md](docs/README.md)
2. Planning notes: `working/` (gitignored)
