# Parser Fixtures

Captured from real appliances via debug-mode ESPHome logs. Each `*.json` is a raw BLE payload; the paired `*.expected.json` is the expected output of `subzero_protocol::parse_*` for that input.

To regenerate an expected file: press the appliance's *Log Debug Info* button (which turns on Debug Mode and lands a fresh poll - subscribe-refresh on fw 2.27, force-reconnect on fw 8.5). Wait ~1.5s on fw 2.27 or ~10-15s on fw 8.5 for the chunked log lines to appear. Then concatenate the `Response[N/M]:` lines from the ESPHome logs (the payload is chunked at 400 bytes to fit the per-line log budget) into `tests/fixtures/<name>.json` and record the entity publish log lines that follow into `<name>.expected.json`. Note that non-printable bytes in the log are replaced with `?` (anti-footgun against non-UTF-8 in HA's protobuf log channel); fixtures should be hand-edited to use the real bytes or captured from a clean poll with no ACL corruption.

## Layout

- `fridge_*` — fed through `parse_fridge`
- `dishwasher_*` — fed through `parse_dishwasher`
- `range_*` — fed through `parse_range` (ranges AND wall ovens)
- `walloven_*` — also fed through `parse_range` (shares the `cav_*` schema)
- `error_*` — malformed / non-OK inputs; expected has `"valid": false`

## Field naming (expected outputs)

Expected outputs flatten the state struct to JSON. Absent keys mean the parser did not populate them. The `valid` key is always present. Nested `common` fields (model, uptime, version, etc.) live under `"common": {...}`.
