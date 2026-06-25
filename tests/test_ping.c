#include <stdio.h>
#include "../include/mr.h"

int dummy_mapper(const mr_file_line_t *line, mr_emit_pair_t emit, void *emit_arg, void *user_arg) {
    (void)line; (void)emit; (void)emit_arg; (void)user_arg;
    return 0;
}

int dummy_reducer(const char *token, const mr_value_t *values, size_t values_count, mr_emit_result_t emit, void *emit_arg, void *user_arg) {
    (void)token; (void)values; (void)values_count; (void)emit; (void)emit_arg; (void)user_arg;
    return 0;
}

int main(void) {
    mr_attr_t attr;
    mr_attr_init(&attr);

    mr_t mr;
    mr_create(&mr, &attr, dummy_mapper, dummy_reducer, NULL);

    mr_start(mr, "input", "output");

    mr_destroy(mr);
    return 0;
}