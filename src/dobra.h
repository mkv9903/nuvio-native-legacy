#ifndef NV_DOBRA_H
#define NV_DOBRA_H

// Provider-styled letters -> ordinary ASCII (#144). Small capitals,
// mathematical, circled, full-width and modifier letters use glyphs that the
// bundled Inter font lacks. Small capitals fold to uppercase. Full-width
// punctuation and styled digits fold too. Return zero for ordinary characters.
// Pure; tested through both implementations by tests/dobra.sh [--rust].
// The legacy C entry-point name remains during migration.
char nv_dobra_estilizada(unsigned long codepoint);

#endif
