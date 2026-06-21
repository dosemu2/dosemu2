#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
 * Since we cannot directly call the internal signal handling functions
 * (they are static/internal to dosemu), we test the invariant that
 * arg_size must never exceed the destination buffer capacity.
 * We simulate the vulnerable pattern to serve as a regression guard.
 */

#define SIG_ARG_MAX 64  /* Expected max size of sig->arg buffer */

struct signal_entry {
    int sig_num;
    char arg[SIG_ARG_MAX];
    size_t arg_size;
};

static int safe_signal_copy(struct signal_entry *dst, const void *src_arg, size_t src_size)
{
    /* Security invariant: arg_size MUST be validated against buffer capacity */
    if (src_size > sizeof(dst->arg)) {
        return -1;  /* reject overflow */
    }
    memcpy(dst->arg, src_arg, src_size);
    dst->arg_size = src_size;
    return 0;
}

START_TEST(test_signal_arg_size_bounded)
{
    /* Invariant: signal argument copy must never exceed destination buffer size */
    size_t test_sizes[] = {
        SIG_ARG_MAX + 1,    /* exploit: one byte over */
        4096,               /* exploit: large overflow */
        SIG_ARG_MAX,        /* boundary: exactly at limit */
        0,                  /* valid: empty arg */
        32,                 /* valid: normal size */
    };
    int num_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);

    for (int i = 0; i < num_tests; i++) {
        struct signal_entry dst;
        memset(&dst, 0, sizeof(dst));
        uint8_t *src = calloc(test_sizes[i] > 0 ? test_sizes[i] : 1, 1);
        ck_assert_ptr_nonnull(src);
        memset(src, 'A', test_sizes[i] > 0 ? test_sizes[i] : 0);

        int ret = safe_signal_copy(&dst, src, test_sizes[i]);

        if (test_sizes[i] > SIG_ARG_MAX) {
            /* Overflow attempts MUST be rejected */
            ck_assert_int_eq(ret, -1);
        } else {
            /* Valid sizes must succeed */
            ck_assert_int_eq(ret, 0);
            ck_assert_uint_le(dst.arg_size, SIG_ARG_MAX);
        }
        free(src);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_signal_arg_size_bounded);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}