#ifndef NUVIO_TEST_PROFILE_CONTEXT_H
#define NUVIO_TEST_PROFILE_CONTEXT_H

/* Fake profile context for C capabilities awaiting their own Rust rewrite.
 * No account, authentication or PIN behavior is tested through this fixture. */
void test_select_profile(int id);

#endif
