# Parser Fixtures

Captured from real appliances via debug-mode ESPHome logs. Each `*.json` is a raw BLE payload; the paired `*.expected.json` is the expected output of `subzero_protocol::parse_*` for that input.

To regenerate an expected file: enable debug mode on the appliance, concatenate the `Response[N/M]:` lines from the ESPHome logs (the payload is chunked at ~600 bytes to fit the task log buffer), save the reassembled JSON here, and record the entity publish log lines that follow. Expected fields use the internal names used by the C++ parser (not the sensor display names).

## Layout

- `fridge_*` — fed through `parse_fridge`
- `dishwasher_*` — fed through `parse_dishwasher`
- `range_*` — fed through `parse_range` (ranges AND wall ovens)
- `walloven_*` — also fed through `parse_range` (shares the `cav_*` schema)
- `error_*` — malformed / non-OK inputs; expected has `"valid": false`

## Field naming (expected outputs)

Expected outputs flatten the state struct to JSON. Absent keys mean the parser did not populate them. The `valid` key is always present. Nested `common` fields (model, uptime, version, etc.) live under `"common": {...}`.
