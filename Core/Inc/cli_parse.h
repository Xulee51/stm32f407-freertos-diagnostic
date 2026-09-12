#ifndef CLI_PARSE_H
#define CLI_PARSE_H

#include <stddef.h>
#include <stdint.h>

/* 节点 10：纯解析层，不依赖 HAL / FreeRTOS，便于主机侧单元测试。 */

#define CLI_PARSE_MAX_ARGS 6U
#define CLI_PARSE_MAX_TOKENS (1U + CLI_PARSE_MAX_ARGS)

typedef enum {
    CLI_CMD_EMPTY = 0,
    CLI_CMD_UNKNOWN,
    CLI_CMD_HELP,
    CLI_CMD_STATUS,
    CLI_CMD_TASKS,
    CLI_CMD_CAN,
    CLI_CMD_TOUCH,
    CLI_CMD_VERSION
} cli_cmd_id_t;

typedef struct {
    cli_cmd_id_t id;
    uint8_t argc; /* 命令名之后的参数个数，0..CLI_PARSE_MAX_ARGS */
    const char *argv[CLI_PARSE_MAX_TOKENS]; /* argv[0] 为命令名，空行时为 NULL */
} cli_parse_result_t;

/*
 * 就地分词：把 line 中的空白改成 NUL，结果里的指针指向 line 内部。
 * 命令名大小写敏感，只接受小写表项。超过 6 个参数时 id 仍按 argv[0]
 * 查表，argc 截断为 6，并返回 1 表示参数过多；正常返回 0。
 */
int cli_parse_line(char *line, cli_parse_result_t *out);
const char *cli_cmd_name(cli_cmd_id_t id);

#endif
