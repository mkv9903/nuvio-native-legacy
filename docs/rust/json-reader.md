# JSON reader migration review

Status: reviewed C implementation and baseline tests; Rust implementation is
pending. The next step migrates `src/js.c` while retaining the C JSON writer
(`jsw`) and existing C callers. The original reader is 347 lines including
comments/blank lines. This record is a starting audit, not a completed port.

## Compatibility boundary

The existing reader is a tolerant byte scanner for known provider/account
formats rather than a complete JSON parser. Preserve its bounded ranges,
borrowed input pointers, key lookup, root-only lookup, array iteration, raw
JSON copying and caller-owned output buffers. Bounded input may have no NUL;
the explicit end pointer must bound all reads.

String decoding handles Unicode escapes and surrogate pairs, ordinary escapes
and the separate newline-preserving mode. Missing/non-string/truncated fields
preserve the destination for the general text reader; the root-only reader
has a different established contract and clears its output. Empty strings and
insufficient output capacity also need explicit compatibility coverage.

Numeric reading accepts quoted ratings, finds usable nested occurrences and
uses a caller fallback for invalid/non-finite values. Timestamp parsing shares
UTC milliseconds with account progress ordering. Preserve those contracts and
validate their platform behavior during migration.

Use English Rust names/documentation and retain the temporary C entry points.
Existing malformed-input tolerance must be represented by tests; intentional
corrections require an explicit expected result and user agreement.

## Baseline and pending truncation decision

On October 11, 2026, both baseline scripts passed with C ASan/UBSan:

```sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash tests/jscadeia.sh
ASAN_OPTIONS=detect_leaks=0 bash tests/js_limites.sh
```

The existing cases cover decoded Unicode/surrogate pairs, quote/backslash/slash
escapes, incomplete strings/escapes, explicitly bounded non-NUL allocations,
numeric ranges/quoted values, root versus nested fields, raw copying and
destination preservation. Carry and translate these cases during the port.
Add real C/Rust comparisons and appropriate account/provider integration tests.

A confirmed C inconsistency needs a user decision before implementation:
with capacity 3 (including NUL), the valid JSON string `"\u65e5AB"` yields
`AB`, because the three-byte escaped character cannot fit and is skipped.
The identical string with the literal character `"日AB"` instead yields an
empty string, stopping before the incomplete UTF-8 character.

Recommendation: Rust should stop at the first decoded character that cannot
fit, retaining the fitting prefix and its NUL rather than copying later text.
Document this as an intentional correction; keep the other tolerant parsing
rules unless a separate decision approves changing them. User choice pending.
