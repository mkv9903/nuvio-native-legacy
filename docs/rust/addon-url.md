# Add-on URL helpers: first Rust module

The Rust Test build now uses `rust/nuvio-core/src/addon_url.rs` for installed
URL normalization, request joining, ID encoding and buffer acceptance rules.
The existing C callers use the adapter in `src/addonurl.h`. Ordinary production
builds continue to use the original C implementation, which is also the parity
test oracle. The broader add-on service, network workers, UI and playback are
still C.

This module uses no external crates, heap allocation, threads, network or file
I/O. Its names, comments, contracts and migrated rejection diagnostics are in
English. Provider-supplied bytes, queries and saved-data fields keep their
existing values; this is not a provider-content translation or data migration.

## Names and bridge

| Existing C entry point | Rust C ABI entry point | Responsibility |
| --- | --- | --- |
| `nv_addon_base` | `nuvio_rust_addon_base` | Normalize an installed URL |
| `nv_addon_url` | `nuvio_rust_addon_request` | Join resource path before the base query |
| `nv_addon_id` | `nuvio_rust_addon_id` | Percent-encode an ID for a resource path |
| `nv_addon_url_cabe` | `nuvio_rust_addon_url_fits` | Accept an installed URL only below 2048 bytes |
| `nv_addon_pedido_coube` | `nuvio_rust_addon_request_fits` | Accept a complete request including its terminator |

The C adapter retains variadic `vsnprintf` formatting for existing request
callers and writes the English rejection messages through the current logger.
Rust owns normalization, joining, encoding and acceptance decisions. Adding
these functions extends ABI version 1 without changing the existing probe
layout. The declaration and ownership contract live in `src/rust_core.h`.

Inputs are borrowed, NUL-terminated byte strings, not necessarily UTF-8. Null
base/ID inputs mean empty strings. Output is writable caller-owned storage of
the stated capacity; inputs and output must not overlap. Null/zero-capacity
output is handled without writing. Rust returns no allocation and retains no
pointer. Calls are synchronous and independent, with no shared mutable state.
Panics abort the process rather than unwinding into C.

## Compatibility rules

- Trim the six C-locale whitespace bytes around an installed URL, including
  vertical tab. Do not apply Unicode whitespace normalization.
- Replace case-insensitive `stremio://` with `https://`. Remove trailing path
  slashes and a case-insensitive terminal `/manifest.json`. Preserve the old
  special case where the entire path is exactly `/manifest.json`.
- Keep a nonempty query byte for byte, including percent escapes, additional
  question marks and fragments. Drop a bare `?` during base normalization.
  Request construction preserves the query it is given and puts the resource
  path before it. This module does not validate URLs or reinterpret fragments.
- Reject an original path that cannot fit before removing its manifest suffix
  or trailing slashes. That intermediate capacity check is existing behavior.
- Keep ASCII letters/digits and `-._~:@!$&'()*+,;=` raw in IDs. Encode every
  other byte as uppercase `%HH`, including each UTF-8 byte and invalid UTF-8.
- A failed or truncated result is empty, never a usable partial URL. Successful
  request joining returns its required byte length without the NUL. A formatted
  path that overflows its 3072-byte scratch buffer retains the C early-return
  length: trimmed base length plus the format result, without the query.
- C formatting can embed NUL bytes. Joining uses the visible path prefix,
  matching the original `%s` behavior rather than the full format result.

Diagnostics identify the add-on and lengths, never its configured URL. The
Rust adapter reports maximum zero for a zero-capacity output, instead of the
legacy unsigned subtraction underflow. Lengths beyond the C `int` range are
rejected with -1 instead of overflowing that type. Current callers use bounded
small buffers, so neither defensive change affects a supported request.

## Verification and iteration

From the repository root:

```bash
bash tests/rust_addon_url.sh
SANITIZE=1 bash tests/rust_addon_url.sh
bash tests/rust_core.sh
bash tools/linux.sh --rust-test --build
bash tools/linux.sh --rust-test --preview
bash tools/arm.sh --rust-test --ipk
```

`tests/rust_addon_url.c` makes 46,786 output/return-value comparisons with the
actual C implementation. Coverage includes long configuration URLs, query and
manifest edge cases, all 255 non-NUL byte values, capacity boundaries, guard
bytes, null outputs, embedded NUL formatting, oversized paths and 1,000
deterministic random input pairs. Direct bridge checks cover format failures
and the acceptance predicates. Rejection messages are checked for English and
for the absence of URL/credential contents.

The same runner also executes the existing `addons.c` integration scenarios
with both C and Rust: installation, export, account-list reload, disk reload,
stream-request construction and oversized-URL rejection. Network services are
test doubles; no real credentials or provider requests are needed.

The sanitizer option instruments the C harness and C reference. Rust is built
with the pinned normal toolchain and retains its bounds checks; it is not an
ASan-instrumented Rust build. In a ptrace-based sandbox where LeakSanitizer
cannot run, use `ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash
tests/rust_addon_url.sh` for the address/undefined-behavior checks.

Linux is the primary iteration environment for this module. ARM linking and
ABI inspection remain required. The two existing physical-TV logs validate
the initial bridge, not this new implementation. A TV spot check can confirm
source/catalog/subtitle requests against installed add-ons when convenient;
a full playback session is not required before the next bounded logic port.
Repeat targeted TV testing for new runtime facilities/dependencies, networking,
threading, storage/lifecycle, remote input, rendering or native playback changes.

New builds print `[rust-core] modules=addon-url` after the startup probe so a
Linux or TV log can distinguish this migration from the earlier probe-only IPK.
The previous package is retained locally under
`build/rust/validation/initial-bridge/`; the root Rust Test IPK is the current
iteration. Both use the same isolated test ID, separate from production.

### Results — October 11, 2026

| Check | Result |
| --- | --- |
| Direct C/Rust parity and C service integration | Passed: 46,786 comparisons and both integration implementations. |
| C ASan/UBSan harness | Passed with `ASAN_OPTIONS=detect_leaks=0`; the sandbox's ptrace prevents LeakSanitizer. Rust itself was not sanitizer-instrumented. |
| Existing C helper bodies | Compared with the branch base; function bodies remain unchanged. |
| Rust/C runtime probe and identity isolation | Passed again with this module linked. |
| Formatting/lint | Stable rustfmt and Clippy with warnings denied passed; compiled artifacts use pinned Rust 1.85.1. ShellCheck and whitespace checks passed. |
| Linux app and graphical smoke check | Built and ran under Xvfb/Mesa llvmpipe; probe and module marker logged, screenshots rendered and Down/Right/Back inputs consumed. Functional evidence, not a TV benchmark. |
| ARM release and ABI inspection | Passed: ARMv7 EABI5 soft-float, GNU loader, glibc symbols through 2.12, unchanged dynamic native library requirements. No TV deployment. |
| IPK verification | Correct test ID/title, executable mode and binary matching the build/ABI hashes, module marker and Rust notices present, no sampled catalog/account/settings/debug payloads. |
| Physical-TV execution of this URL port | Pending an optional spot check. The earlier logs establish the probe-only hybrid, not this module. |

Release build ID: `821b9e96dbb9b8944a34f7dacf371ecce2395c86`. Symbol and ABI
evidence is under
`build/webos-rust/eff1a0b95542-dirty/821b9e96dbb9b8944a34f7dacf371ecce2395c86/release/`.
The IPK SHA-256 is
`24ddfbc89c9f3a5ab0ff98986a4214198852e2ea1a04f8fd04b349109ae8fdb5`.
These identify the first helper-port artifact; rebuilding a changed workspace
can replace the root package and update the latest symbol pointer.

## Next boundary

Inventory and migrate one provider response parser/model next, with fixtures
for malformed, missing and Unicode data and explicit ownership of returned
records. Keep C networking and the UI connected through the existing service
interface. Provider workflows, account/sync, persistence and finally UI move
after their own parity checks; this helper port does not mark those complete.
