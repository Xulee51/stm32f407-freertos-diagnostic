#include "cli_parse.h"

#include <string.h>

typedef struct {
    const char *name;
    cli_cmd_id_t id;
} cli_cmd_entry_t;

static const cli_cmd_entry_t cli_cmd_table[] = {
    {"help", CLI_CMD_HELP},
    {"status", CLI_CMD_STATUS},
    {"tasks", CLI_CMD_TASKS},
    {"can", CLI_CMD_CAN},
    {"touch", CLI_CMD_TOUCH},
    {"version", CLI_CMD_VERSION}
};

static int is_space(char c)
{
    return (c == ' ') || (c == '\t');
}

static cli_cmd_id_t lookup_cmd(const char *name)
{
    size_t i;
    if (name == NULL) {
        return CLI_CMD_EMPTY;
    }
    for (i = 0U; i < (sizeof(cli_cmd_table) / sizeof(cli_cmd_table[0])); ++i) {
        if (strcmp(name, cli_cmd_table[i].name) == 0) {
            return cli_cmd_table[i].id;
        }
    }
    return CLI_CMD_UNKNOWN;
}

const char *cli_cmd_name(cli_cmd_id_t id)
{
    size_t i;
    for (i = 0U; i < (sizeof(cli_cmd_table) / sizeof(cli_cmd_table[0])); ++i) {
        if (cli_cmd_table[i].id == id) {
            return cli_cmd_table[i].name;
        }
    }
    if (id == CLI_CMD_EMPTY) {
        return "";
    }
    return "?";
}

int cli_parse_line(char *line, cli_parse_result_t *out)
{
    size_t token_count = 0U;
    int too_many = 0;
    char *p;

    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->id = CLI_CMD_EMPTY;
    if (line == NULL) {
        return 0;
    }

    p = line;
    while (*p != '\0') {
        while (is_space(*p) != 0) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (token_count < CLI_PARSE_MAX_TOKENS) {
            out->argv[token_count] = p;
            token_count++;
        } else {
            too_many = 1;
        }
        while ((*p != '\0') && (is_space(*p) == 0)) {
            p++;
        }
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }

    if (token_count == 0U) {
        out->id = CLI_CMD_EMPTY;
        out->argc = 0U;
        return 0;
    }

    out->id = lookup_cmd(out->argv[0]);
    out->argc = (uint8_t)(token_count - 1U);
    if (too_many != 0) {
        out->argc = CLI_PARSE_MAX_ARGS;
        return 1;
    }
    return 0;
}
