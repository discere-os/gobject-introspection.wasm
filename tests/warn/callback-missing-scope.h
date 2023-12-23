#include "common.h"

void test_callback(GCallback *callback, gpointer user_data);

// EXPECT:3: Warning: Test: test_callback: argument callback: Missing (scope) annotation for callback without GDestroyNotify (valid: call, async, forever)

/**
 * test_callback_with_scope:
 * @callback: (scope call):
 * @user_data:
 */
void test_callback_with_scope(GCallback callback, gpointer user_data);

/**
 * test_callback_missing_scope_shadowed:
 * @callback:
 * @user_data:
 */
void test_callback_missing_scope_shadowed(GCallback callback, gpointer user_data);

/**
 * test_callback_missing_scope_shadowed_full: (rename-to test_callback_missing_scope_shadowed)
 * @callback:
 * @user_data:
 * @destroy_notify:
 */
void test_callback_missing_scope_shadowed_full(GCallback callback, gpointer user_data, GDestroyNotify destroy_notify);
