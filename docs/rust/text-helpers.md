# Text cleanup and local search

The second helper batch migrates `dobra`, `limpa` and `buscanorm` into the Rust
core. The existing C UI, source-list service and font renderer call the same C
entry points; `NV_RUST_CORE` selects small forwarding adapters. Production C
builds retain the original implementations, also used as regression references.
No new dependency, allocation, I/O, worker or platform API is introduced by
these helpers. The existing startup probe still tests the Rust runtime.

## English names and compatibility

| Existing C entry point | Rust implementation/export |
| --- | --- |
| `nv_dobra_estilizada` | `stylized::fold_stylized_character` / `nuvio_rust_fold_stylized_character` |
| `nv_limpar_texto` | `text_cleanup::cleanup` / `nuvio_rust_text_cleanup` |
| `nv_aparar_separadores` | `text_cleanup::trim_separators` / `nuvio_rust_text_trim_separators` |
| `busca_normalizar` | `search_normalization` / `nuvio_rust_search_normalize` |
| `busca_codepoints` | `nuvio_rust_search_codepoints` |
| `busca_apagar_ultimo` | `nuvio_rust_search_delete_last` |

Rust code, the adapter contracts, migrated test names/comments/diagnostics and
this documentation use English. Existing ABI names and the single-line flag
value remain compatible. Corpus content is unchanged, including multilingual
titles, language markers and deliberately malformed byte fixtures. English UI
does not restrict the languages of provider content, audio or subtitles.

The stylized mapping retains small capitals, modifiers, full-width punctuation,
circled/squared letters, superscript/subscript digits and the mathematical
block's historical arithmetic mapping, including unassigned holes. Its C
`unsigned long` parameter uses Rust `c_ulong` for ARM32 and the Linux host.

Cleanup retains the existing bundled-font policy: styled characters become
ASCII; flags become language/country codes; supported check/cross/star/arrow
symbols map to existing glyphs; decoration becomes a middle-dot separator.
Control/invalid bytes and unsupported characters disappear. Valid BMP scripts
retain the existing fallback-font behavior; this port does not expand the font
inventory or the non-BMP display policy. Single-line and multiline spacing,
empty decoration-only bracket removal and separator trimming stay compatible.
Rust keeps replacement bytes local to each call, removing C cleanup's shared
mutable one-character replacement buffer.

Local search retains the Latin/Romanian accent and Cyrillic case rules. Other
valid scripts pass through unchanged. Provider requests continue receiving the
original typed search term. Character counting and backspace retain their
non-continuation-byte rules, including for malformed input; those helpers have
not been changed to count grapheme clusters.

## Approved malformed UTF-8 correction

On October 11, 2026, the user approved rejecting overlong encodings, surrogate
values and values above U+10FFFF during Rust search normalization. Each invalid
byte becomes one ASCII space. For example, `A E0 80 80 Z` becomes `a   z` rather
than containing an embedded NUL from an overlong encoding. Existing C search
checked continuation structure without checking these scalar constraints.

All valid UTF-8 retains its existing behavior. A literal, valid U+FFFD remains
in search output; it differs from an invalid byte. Cleanup already rejected
these encodings and continues to drop invalid bytes. The legacy production C
search implementation remains unchanged until its replacement is complete.

## Buffer and ownership contracts

Inputs are readable NUL-terminated byte strings. Null input is empty. Caller
outputs include space for the NUL; null/zero-capacity output writes nothing.
Truncation happens before a complete character; cleanup also commits its
pending separator/line-break prefix together with the next visible character.
Cleanup output must not overlap input. Search supports disjoint buffers and
exact same-address normalization; partial overlap is unsupported. It stages
each character before writing and never expands beyond consumed input bytes.

Separator trimming operates in place on a writable NUL-terminated line and
never grows it. Backspace needs writable storage through the supplied current
byte length inclusive. Rust accepts null editing buffers defensively, returning
zero; legacy C callers already supply valid storage. No allocations or pointers
cross or survive the ABI. The additive exports retain core ABI version 1.

## Automated validation

Run the full batch with the pinned toolchain and actual Rust static library:

```sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash tests/rust_text_helpers.sh
```

The sandbox cannot run LeakSanitizer under its tracing environment; the setting
above disables only leak detection. ASan/UBSan instrument the C harness,
adapters and reference code, not the prebuilt Rust standard/core library.
The cleanup harness now frees its own corpus/font allocations.

Results: eight Rust unit tests and **4,611,963 comparisons** pass. Styled
mapping is checked for every value U+0000 through U+10FFFF and `ULONG_MAX`.
Every non-NUL valid Unicode scalar is compared through cleanup and search C
adapters; search is also checked in place. Seeded valid/malformed mixtures,
small capacities, guard bytes, flag/punctuation interactions, separator
trimming, counting, backspace and null contracts are covered.

Of those cases, 4,235 search inputs/capacities use the approved strict rule:
an independent C byte-range validator replaces invalid bytes with spaces
before asking the original C search algorithm for the expected folding.
These are documented correction checks rather than claims of unchanged C
behavior. Fixed Rust regressions explicitly test overlongs, surrogates,
out-of-range encodings and literal U+FFFD. Parallel cleanup tests exercise
different styled replacements without shared mutable state.

The original `dobra`, `buscanorm` and cleanup corpus/font-glyph cases run
against both C and Rust, including cleanup idempotence, capacities 1–63 and
standalone/edge separator cases. They remain individually runnable:

```sh
bash tests/dobra.sh --rust
bash tests/buscanorm.sh --rust
ASAN_OPTIONS=detect_leaks=0 bash tests/limpa.sh --rust
```

## Integrated validation and user check

Linux and ARM release builds pass with the pinned Rust 1.85.1 library. The
Xvfb/Mesa llvmpipe preview logged a passing runtime probe and the complete
module marker, rendered screenshots and consumed Down/Right/Back navigation.
A second check preseeded only three first-run tutorial flags in temporary test
data, opened Spotlight and typed `fallout`; its search field rendered correctly.
This smoke check does not establish provider-backed result/source coverage or
TV performance. Source cleanup is covered by the automated font corpus; real
source-list appearance remains part of the user check below.

```text
[rust-core] modules=addon-url,stylized,text-cleanup,search-normalization
```

ARM inspection before and after stripping passes: EABI5 soft-float, GNU loader
`/lib/ld-linux.so.3`, GLIBC symbols through 2.12, and exactly the previous build's
dynamic native dependencies. No new runtime library is required. Physical-TV
execution of this helper batch has not yet been performed.

The historical text-batch package was built from clean commit `cead05c2`.
The root IPK is now superseded by the [JSON reader build](json-reader.md);
the following hashes identify the earlier text artifact:

- Package: `space.mkv.nuvio.rusttest_2.1.0_arm.ipk`, 9,111,578 bytes.
- Package SHA-256: `7bbead9f04a98017943af90df1537103570017aaffe68cf4ef26db0101b79c13`.
- Binary SHA-256: `cfede84aa0c722d6b1d905ce5fe0644e812a878a79c513b539b4c4b0ac3d4822`.
- Build ID: `8a7cfd68fd2817c1f9504ea0d0bf5f9d64f6afe0`.
- Symbols/ABI record: `build/webos-rust/cead05c22dce/8a7cfd68fd2817c1f9504ea0d0bf5f9d64f6afe0/release/`.

Package verification compared all 601 runtime files with the clean allowlisted
staging tree. Test ID/title, executable permissions, built binary hash, module
marker, package metadata and Rust license notices match. No account/settings
files, sampled catalog/artwork or developer symbols are packaged. Local
validation evidence is under `build/rust/validation/`; generated files and the
IPK are ignored by Git. Formatting, Clippy with warnings denied, ShellCheck and
whitespace checks pass.

For this pure helper batch, user testing should focus on the Linux preview:
search accented titles, enter/remove non-ASCII characters with the on-screen
delete key or the desktop **Delete** key, and
inspect available source names/descriptions for broken symbols or spacing.
The existing Linux UI maps physical Backspace to Back; this batch retains its
input handling. Dismiss first-run cards before testing search. For account-backed
rows, add-ons and source descriptions, use the normal Linux mode after pulling
`rust-rewrite`:

```sh
NUVIO_UI_PREVIEW=0 bash tools/linux.sh --rust-test
```

Sign in to Rust Test separately, select your profile and let account sync load
its rows/add-ons. Linux playback remains stubbed. The `--preview` flag is an
optional account-free mode that skips login/account sync and uses sample UI
content; it cannot validate account-backed rows. It also uses a different
default test data directory from normal Linux mode.

No new full TV playback session is required for these helpers. A packaged
Rust Test IPK accompanies the handoff for an optional TV smoke check.
Native playback remains subject to TV validation when later work changes it.

On October 11, 2026, the user tested normal Linux Rust Test mode and confirmed
that account catalogs loaded correctly. This provides user validation of the
account-backed Linux path with the text helpers linked. Individual accented
search/source-display checks were not separately reported. The Linux search
shortcut is **F5**; **F6** opens the microphone shortcut. Core migration proceeds
with the [JSON reader](json-reader.md).
