# JSON writer migration

Status: Rust Test now delegates all JSON emission in `src/jsw.c` to
`rust/nuvio-core/src/json_writer.rs`. Both JSON reader and writer are Rust;
account/sync/provider callers and the UI remain C. The unchanged original
writer stays under the non-Rust build path as the compatibility oracle.
The original writer was 149 C lines including comments and blank lines.

## Ownership and ABI

The existing caller-owned `Jsw` structure keeps its layout and public C names.
Rust uses English names: `JsonWriter` with `buffer`, `length`, `capacity`,
`error`, `depth` and `first`. C adapters and header documentation are English;
the temporary C fields `p`, `n`, `cap`, `erro`, `prof` and `primeiro` remain
compatibility names. Adding the `Jsw` struct tag changes no storage layout.

C callers initialize the context, emit JSON and borrow the finished output before
freeing/resetting the context. No live writer may be copied by value. Inputs
are NUL-terminated bytes disjoint from the context/output buffer; no input
pointer is retained. A mutation may reallocate output and invalidates earlier
borrowed output pointers. Initialization requires storage with no live buffer;
free/reset may be repeated, and the reset context can be reused.

Rust calls the same native `realloc`/`free` as the original C writer. The
context owns that native buffer; this is not a Rust `Vec`/`String` allocation
exported for C to dispose of. Existing `session.c` password-body wiping can
still mutate the borrowed bytes before free. There is no allocator mismatch,
new runtime dependency, I/O, shared writer state or automatic Rust drop across
the ABI. The probe ABI remains version 1 with additive writer exports.

The native layouts are checked at compile time on both sides: 48 bytes on
64-bit GNU Linux and 36 on 32-bit GNU ARM, including every field offset and
the 16-entry nesting stack. Future platform adapters need their own validation.

| Existing C API | Rust implementation |
| --- | --- |
| `jsw_iniciar`, `jsw_livre`, `jsw_texto_final` | `nuvio_rust_json_writer_init`, `free`, `finish` |
| `jsw_obj_ini`, `jsw_arr_ini` | `nuvio_rust_json_writer_open` with object/array delimiter |
| `jsw_obj_fim`, `jsw_arr_fim` | `nuvio_rust_json_writer_close` with object/array delimiter |
| `jsw_chave`, `jsw_str` | `nuvio_rust_json_writer_key`, `string` |
| `jsw_int`, `jsw_bool`, `jsw_nulo` | `nuvio_rust_json_writer_integer`, `boolean`, `null` |
| `jsw_bruto` | `nuvio_rust_json_writer_raw` |
| `jsw_cs`, `jsw_ci`, `jsw_cb` | Small C delegates compose the Rust key/value operations |

## Preserved behavior

Separators are automatic within each nesting level. A key consumes the level's
separator and resets its first-value flag so its value adds no second comma.
This preserves the historical regression fix for invalid bodies such as
`{,"a":1}`. Objects/arrays may nest up to 16 levels, and output is NUL-terminated
after every successful append. Allocation starts at 256 bytes and doubles
at the same capacity boundaries as C.

Strings escape quotes, backslashes, newline, CR and tab. Other control bytes
below 0x20 use lowercase `\u00xx`. Non-ASCII bytes pass through unchanged,
including the established malformed-byte policy: the writer does not validate
or normalize UTF-8. Input NUL ends a string. Null string inputs emit JSON null;
null keys become empty keys. Raw JSON is forwarded unchanged without
validation; null/empty raw input emits JSON null. Protocol/saved-data keys and
multilingual content are retained.

Signed 64-bit integers, including the minimum value, are emitted with a bounded
20-byte stack formatter using ASCII decimal digits. This matches the old
`%lld` output without an intermediate heap allocation or locale-sensitive
formatting. Any nonzero boolean value is true.

Allocation failure or excessive nesting latches the error. Finished output
then returns NULL and later appends emit no more bytes. The original C API
still updates some depth/first bookkeeping after an error; that behavior is
preserved too, rather than treating its old "no-op" comment as a new rule.
This API still does not validate mismatched closing delimiters, incomplete
objects, multiple root values or caller-supplied raw JSON. Caller behavior
and output stay compatible. Arithmetic overflow or an allocation beyond
Rust's pointer-size limit fails with the same sticky error instead of
wrapping/panicking; those sizes were unsafe in the old C growth calculation.

## Tests

```sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash tests/rust_json_writer.sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash tests/json_writer.sh --rust
```

The old writer had no dedicated standalone regression runner. This migration
adds an English C API suite runnable against either implementation, retains
the unchanged C writer as an oracle, and adds four Rust unit cases. The full
core has 16 passing unit tests with pinned Rust 1.85.1. Cases cover escaping,
nesting/separators, numeric boundaries, null/empty/raw values, buffer growth,
error bookkeeping, release/reset/reuse and independent concurrent writers.

The Rust API suite passes 1,771,128 assertions/comparisons; the C baseline
passes 1,771,127 (Rust adds a defensive null-context check). These counts include
byte/state and allocator-guard assertions, not that many distinct documents.
Each step compares output, length, capacity, error, nesting and first-value
flags with C. All input byte values 1–255, representative multilingual text,
500 deterministic 80-operation sequences, 2,000 signed integer values,
capacity boundaries and depth limits are covered. Fixed expected documents
independently check escaped output and the double-comma regression.

The C harness wraps the real native `realloc`/`free` calls from both languages.
Guard suffixes detect writes beyond the requested capacity, including Rust
writes that ASan does not instrument. Nine controlled allocation budgets
exercise initial allocation failure and failures during growth, compare
partial state with C, and verify cleanup/reuse. Outstanding allocations must
be gone at suite completion. A borrowed password body is wiped and freed,
and a C caller also directly releases a native writer buffer. ASan/UBSan cover
the C harness/adapters/oracle; this is not Rust sanitizer instrumentation.
Leak detection is disabled for the sandbox's tracing limitation.

Existing account-pending tests pass using Rust reader/writer against their
stateful fake server: watched/saved operations, offline retry, profile isolation
and preservation of remote list entries. Trakt refresh/profile and Simkl
profile tests also pass with Rust JSON. The reader integration suite exercises
profile PIN/selection/persistence and progress-sync request bodies with both
JSON modules now in Rust. These tests access no real account or network.
Neighboring module fixtures stay with those modules until their own migration.

The earlier suites pass alongside this batch: 293,723,352 reader assertions/
comparisons and its account/provider integrations, 46,786 URL comparisons and
both consumer variants, and 4,611,963 text comparisons plus retained C/Rust
regressions. Rust formatting, Clippy with warnings denied, ShellCheck and
whitespace checks pass. Integrated release results are below.

## Integrated release validation

Pinned Rust 1.85.1 Linux and ARM release builds pass from clean code commit
`ffa37558`. The isolated Xvfb/Mesa llvmpipe preview logged a passing probe and
the full module marker, rendered Home and Search, and consumed search opening
and typed `fallout`. The screenshot showed populated titles, artwork and
ratings. This account-free smoke check does not establish real account sync
or TV performance; the account-writer paths use the automated consumers above
and remain part of the user's next Linux check.

```text
[rust-core] app=space.mkv.nuvio.rusttest ABI=1 probe=PASS status=0 checks=7 checksum=522240
[rust-core] modules=addon-url,stylized,text-cleanup,search-normalization,json-reader,json-writer
```

ARM inspection before/after stripping passes: ARMv7 EABI5 soft-float, GNU
loader `/lib/ld-linux.so.3`, GLIBC symbols through 2.12 and exactly the previous
hybrid build's native dependencies. The shared writer-layout assertions
compile on both targets. No new runtime library is introduced.

- Package: `space.mkv.nuvio.rusttest_2.1.0_arm.ipk`, 9,113,494 bytes.
- Package SHA-256: `dc4aa4dabdab68ffd68eed43894f5ecf3547349e93303d355f9222ad6f52c236`.
- Binary SHA-256: `5a7ac4f1afdabdfd0864a513801738b7af317a662b04c5b6047295c5b0d62db6`.
- Build ID: `9fa58316f273c5934cbed79476d2b8a3a9621f4c`.
- Symbols/ABI record: `build/webos-rust/ffa3755845e0/9fa58316f273c5934cbed79476d2b8a3a9621f4c/release/`.

Verification compared all 601 runtime files with clean allowlisted staging.
Test identity/title, executable permissions, current binary hash, full module
marker, metadata and Rust notices match. No account/settings files, sample
catalog/artwork or developer symbols are packaged. The root IPK replaces the
reader-milestone artifact; these hashes and the source/symbol record identify
the writer build. Generated evidence is under `build/rust/validation/` and
ignored by Git. Physical-TV execution of this build remains unverified.

## User validation

The user confirmed on October 11, 2026 that the previous JSON reader's Linux
test looked fine and authorized the writer substep. Individual checks/logs and
a reader TV run were not separately reported.

After pulling `rust-rewrite`, use account-enabled Linux mode:

```sh
NUVIO_UI_PREVIEW=0 bash tools/linux.sh --rust-test
```

Check login/account rows and profile switching. Add/remove a test title in
Saved and mark/unmark it watched, then close/reopen and confirm those changes
sync/persist. Restore the test title to its original state afterward. Test a
PIN-protected profile if your account has one. Linux playback remains stubbed;
`--preview` skips login/account sync and cannot validate these writer consumers.

This batch changes JSON emission used by account/authentication/sync, without
changing native media commands or the playback pipeline. Linux is the main
user check; no new full TV playback session is required for this writer port.
An isolated Rust Test IPK accompanies the handoff for an optional TV account
check. The reader's earlier targeted TV check remains pending. Next comes
source-model parsing/filtering and pure badge/audio labels as bounded steps.
