#include "cli_parse.h"

#include <stdio.h>
#include <string.h>

static int g_failed;

static void expect_eq_int(const char *name, int got, int want)
{
    if (got != want) {
        (void)printf("FAIL %s: got %d want %d\n", name, got, want);
        g_failed = 1;
    }
}

static void expect_eq_str(const char *name, const char *got, const char *want)
{
    if ((got == NULL) || (want == NULL) || (strcmp(got, want) != 0)) {
        (void)printf("FAIL %s: got \"%s\" want \"%s\"\n",
                     name,
                     (got != NULL) ? got : "(null)",
                     (want != NULL) ? want : "(null)");
        g_failed = 1;
    }
}

static void parse_copy(const char *src, cli_parse_result_t *out, int *too_many)
{
    char line[96];
    (void)snprintf(line, sizeof(line), "%s", src);
    *too_many = cli_parse_line(line, out);
}

int main(void)
{
    cli_parse_result_t r;
    int too_many;

    parse_copy("", &r, &too_many);
    expect_eq_int("empty id", (int)r.id, (int)CLI_CMD_EMPTY);
    expect_eq_int("empty argc", (int)r.argc, 0);
    expect_eq_int("empty too_many", too_many, 0);

    parse_copy("   \t  ", &r, &too_many);
    expect_eq_int("spaces id", (int)r.id, (int)CLI_CMD_EMPTY);

    parse_copy("help", &r, &too_many);
    expect_eq_int("help id", (int)r.id, (int)CLI_CMD_HELP);
    expect_eq_int("help argc", (int)r.argc, 0);
    expect_eq_str("help argv0", r.argv[0], "help");
    expect_eq_str("help name", cli_cmd_name(r.id), "help");

    parse_copy("  status  ", &r, &too_many);
    expect_eq_int("status id", (int)r.id, (int)CLI_CMD_STATUS);
    expect_eq_int("status argc", (int)r.argc, 0);

    parse_copy("tasks a b", &r, &too_many);
    expect_eq_int("tasks id", (int)r.id, (int)CLI_CMD_TASKS);
    expect_eq_int("tasks argc", (int)r.argc, 2);
    expect_eq_str("tasks arg1", r.argv[1], "a");
    expect_eq_str("tasks arg2", r.argv[2], "b");

    parse_copy("can", &r, &too_many);
    expect_eq_int("can id", (int)r.id, (int)CLI_CMD_CAN);
    parse_copy("touch", &r, &too_many);
    expect_eq_int("touch id", (int)r.id, (int)CLI_CMD_TOUCH);
    parse_copy("version", &r, &too_many);
    expect_eq_int("version id", (int)r.id, (int)CLI_CMD_VERSION);

    parse_copy("HELP", &r, &too_many);
    expect_eq_int("HELP unknown", (int)r.id, (int)CLI_CMD_UNKNOWN);
    expect_eq_str("HELP argv0", r.argv[0], "HELP");

    parse_copy("foo", &r, &too_many);
    expect_eq_int("foo unknown", (int)r.id, (int)CLI_CMD_UNKNOWN);

    parse_copy("help 1 2 3 4 5 6", &r, &too_many);
    expect_eq_int("six args", (int)r.argc, 6);
    expect_eq_int("six too_many", too_many, 0);
    expect_eq_str("six last", r.argv[6], "6");

    parse_copy("help 1 2 3 4 5 6 7", &r, &too_many);
    expect_eq_int("seven too_many", too_many, 1);
    expect_eq_int("seven argc cap", (int)r.argc, 6);
    expect_eq_int("seven still help", (int)r.id, (int)CLI_CMD_HELP);

    if (g_failed != 0) {
        (void)printf("cli_parse tests FAILED\n");
        return 1;
    }
    (void)printf("cli_parse tests OK\n");
    return 0;
}
