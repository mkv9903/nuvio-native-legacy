# JSON reader migration

Status: the Rust Test build now uses the Rust reader through the existing C
entry points in `src/js.c`. The original C implementation remains unchanged
under the non-Rust build path. The [JSON writer](json-writer.md) has since
migrated too; account, provider, UI and playback callers remain C.

## Compatibility boundary

`rust/nuvio-core/src/json_reader.rs` scans the application's known JSON formats.
It preserves the existing tolerant byte reader rather than introducing a full
JSON validator or a third-party dependency. All buffers belong to the caller;
no Rust allocation or retained pointer crosses the ABI. Output buffers must
not overlap readable input/key buffers. Returned array/container pointers borrow
the original input.

An explicit `[start,end)` range may contain NUL and need no terminator at its
end. Every key and standalone input is NUL-terminated. A null range end selects
the existing NUL-terminated mode. Bounded string, numeric and raw readers never
read beyond the explicit end. Container-end and array-advance helpers walk only
the relevant element/prefix: they do not measure the remaining document first.
This retains linear traversal of account arrays.

| Existing C entry point | Rust implementation |
| --- | --- |
| `js_fim` | `nuvio_rust_json_end` |
| `js_tem` | `nuvio_rust_json_has` |
| `js_cadeia` | `nuvio_rust_json_string` |
| `js_texto`, `js_texto_linhas` | `nuvio_rust_json_text`, ordinary/keep-lines mode |
| `js_texto_raiz`, `js_texto_raiz_em` | `nuvio_rust_json_text`, root-only mode |
| `js_num` | `nuvio_rust_json_number` |
| `js_array` | `nuvio_rust_json_array` |
| `js_prox` | `nuvio_rust_json_next` |
| `js_raiz_array` | `nuvio_rust_json_root_array` |
| `js_bruto` | `nuvio_rust_json_raw` |
| `js_ms_iso` | `nuvio_rust_json_iso_ms` |

Public C names and protocol keys remain compatible; Rust names, adapters,
header documentation and migrated test descriptions are English. Multilingual
provider values remain valid content. ABI version 1 gains additive functions.

## Preserved tolerant behavior

General lookup returns the first matching raw key outside quoted values,
including nested occurrences. Root-only lookup checks depth-one keys in the
first object. These distinct rules avoid replacing manifest IDs/catalog names
with nested values. Keys are compared as raw bytes, without decoding escapes.

Missing, non-string or incomplete string values preserve general-reader
output. Root-only lookup clears output before lookup for valid non-null
arguments. Empty decoded strings write NUL and return 0; a nonempty fitting
prefix returns 1. Raw JSON copying preserves output unless the complete value
fits, including its brackets/braces/string quotes.

Unicode escapes decode to UTF-8, combining valid surrogate pairs. Lone
surrogates become spaces. Invalid hex emits a space and leaves its hex bytes
for literal copying, as before. Escaped NUL remains an embedded output NUL.
Ordinary mode replaces escaped newline/CR/tab with spaces; keep-lines mode
retains newline, drops CR and substitutes a space for tab. Unknown escapes
retain their escaped character. Literal malformed UTF-8 retains the original
byte policy; the previously approved strict search-normalization rule does
not silently change provider JSON parsing.

Numeric lookup accepts quoted ratings and searches past nonnumeric values
for another occurrence. It preserves native `strtod` prefix conversion,
finite-value checks, the 128-byte temporary limit and caller fallbacks.
Timestamp conversion retains the existing `sscanf` date format and native
GNU `timegm` calendar normalization/range, including 32-bit `time_t` on the TV.
It interprets the value as UTC and ignores zone suffixes, as the old reader did;
this is not a general ISO-8601 offset parser. At most three fractional digits
are kept. Rust uses these existing libc primitives without a new runtime
library. C and Rust compile-time checks validate the native `struct tm` layout
and time width on the current GNU Linux/ARM targets. Other platform adapters
will require separate validation when cross-platform work begins.

## Approved truncation correction

On October 11, 2026, the user approved keeping only the fitting decoded prefix.
With capacity 3 (including NUL), C reads `"\u65e5AB"` as `AB`, skipping the
three-byte character that cannot fit. Rust returns an empty string, matching
the literal form `"日AB"`. With `"P\u65e5AB"` and capacity 3, Rust returns `P`.
It stops before the first decoded character that cannot fit instead of copying
later text. All other established tolerant rules above are retained.

This correction is covered independently of the unchanged C oracle. It is
not counted as a claim of exact legacy behavior. There is no saved-data or
provider-protocol migration.

## Reproducible verification

```sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash tests/rust_json_reader.sh
ASAN_OPTIONS=detect_leaks=0 bash tests/jscadeia.sh --rust
ASAN_OPTIONS=detect_leaks=0 bash tests/js_limites.sh --rust
```

The migration retains and translates both existing reader regression suites,
which run against C and Rust. Four new Rust unit tests exercise decoded
prefixes, tolerant escapes/embedded NUL, newline handling, root lookup and
native numeric/date conversion. The complete core at the reader milestone had 12 Rust unit
tests, all passing with pinned Rust 1.85.1.

The direct C/Rust harness passes 293,723,352 assertions/comparisons. It checks
every Unicode scalar with both implementations at sufficient capacity and
independently checks fitting prefixes/capacity guards for each scalar. Other
cases cover every prefix of representative documents, exact-size non-NUL
allocations, root/nested fields, raw copying, destination preservation,
embedded NUL, quoted/nested/non-finite/oversized numbers, permissive and
normalized dates, native 2038 boundaries and 3,000 deterministic malformed
byte inputs. Explicit ranges also end at a protected memory page, so an
out-of-range Rust read faults even though Rust is not ASan-instrumented.
ASan/UBSan instruments the C adapters, reference and consumers; this is not a
claim of Rust sanitizer instrumentation. Leak detection is disabled for this
sandbox's tracing limitation.

Representative existing consumers pass using the actual Rust reader: profile
selection/persistence across two processes, account library/watched episodes,
progress sync, Xtream channel/account/EPG parsing, stream metadata and add-on
installation/export/reload. Their dependencies use the existing test doubles;
no real account or network is accessed by these tests. Neighboring modules'
fixtures remain with those modules until their own migration.

The earlier URL suite still passes all 46,786 comparisons and both consumer
variants. Text helpers still pass 4,611,963 comparisons and their C/Rust
regressions. Rust formatting, Clippy with warnings denied, ShellCheck and
whitespace checks pass. Integrated release results are recorded below.

## Integrated release validation

Linux and ARM release builds pass with the pinned Rust 1.85.1 library, built
from clean code commit `f9a824ec`. The isolated Xvfb/Mesa llvmpipe run logged a
passing runtime probe with all migrated modules, rendered Home and Search,
and consumed search opening and typed `fallout`. The captured search screen
showed populated titles, artwork and ratings. This account-free run does not
establish real account sync, source playback or TV performance.

```text
[rust-core] app=space.mkv.nuvio.rusttest ABI=1 probe=PASS status=0 checks=7 checksum=522240
[rust-core] modules=addon-url,stylized,text-cleanup,search-normalization,json-reader
```

ARM inspection before/after stripping passes: EABI5 soft-float, GNU loader
`/lib/ld-linux.so.3`, GLIBC symbols through 2.12 and exactly the previous
hybrid build's native library dependencies. GNU date-layout assertions also
compile on both targets. No new runtime library is introduced.

- Package: `space.mkv.nuvio.rusttest_2.1.0_arm.ipk`, 9,112,728 bytes.
- Package SHA-256: `b71385924cca2be2527df36ed6930949fbd9bec697ef9e0c2fa3748ad84165a0`.
- Binary SHA-256: `f382c87ada39247ab7bdb088eb419a412809ea3fb41ea6ec200ac57ef3ef99e2`.
- Build ID: `d2b052ac01e810ab513389e86e670d6316b69e04`.
- Symbols/ABI record: `build/webos-rust/f9a824ecdbee/d2b052ac01e810ab513389e86e670d6316b69e04/release/`.

Package verification compares all 601 runtime files with the clean
allowlisted staging tree. Test ID/title (`space.mkv.nuvio.rusttest`, **Nuvio
Rust Test**), executable permissions, current binary hash, full module marker,
metadata and Rust notices match. No account/settings files, sample catalog/art
or developer symbols are packaged. The root IPK filename replaces the earlier
text-batch artifact; the source commit, hashes and symbol archive identify
this historical reader build, now superseded by the writer IPK. Local evidence is under `build/rust/validation/`
and ignored by Git. Physical-TV execution remains pending.

## User validation

After pulling `rust-rewrite`, use the account-enabled Linux mode:

```sh
NUVIO_UI_PREVIEW=0 bash tools/linux.sh --rust-test
```

Check that your profile and account rows/add-ons load, then open search with
**F5** (or **Fn+F5**), a title and its sources. Check names, descriptions,
ratings and multilingual text, including multiline source descriptions.
Close/relaunch and confirm login/profile state persists. `--preview` skips
account login/sync and is only the optional sample-content UI mode. Linux
playback remains stubbed.

The JSON reader also supplies native playback/service responses. A targeted
TV check of one familiar video, source selection, subtitle list and saved
resume position is appropriate for this broadly used parser, even though the
playback pipeline itself has not changed. A full codec/performance matrix is
not required for this batch. Physical-TV execution of this reader remains
pending until the user reports it.

On October 11, 2026, the user reported that the Linux test looked fine and
authorized the writer substep. No individual test results or new TV log were
provided; physical-TV reader validation remains pending.
