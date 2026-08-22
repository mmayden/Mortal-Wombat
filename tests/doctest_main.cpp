// doctest's main(), compiled exactly once and shared by every test tier.
// Splitting it out keeps each tier's own translation units fast to rebuild,
// which is what keeps the whole suite inside the ADR 0010 budget.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
