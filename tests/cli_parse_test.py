#!/usr/bin/env python3
"""Host-side checks for CLI parse rules.

This file mirrors Core/Src/cli_parse.c so the tests can run without a
native gcc (this project uses ARM GCC only). If a host gcc is available,
also run tools/run_cli_parse_tests.sh against the C sources.
"""

from __future__ import annotations

CLI_PARSE_MAX_ARGS = 6
CLI_PARSE_MAX_TOKENS = 1 + CLI_PARSE_MAX_ARGS

CLI_CMD_EMPTY = 0
CLI_CMD_UNKNOWN = 1
CLI_CMD_HELP = 2
CLI_CMD_STATUS = 3
CLI_CMD_TASKS = 4
CLI_CMD_CAN = 5
CLI_CMD_TOUCH = 6
CLI_CMD_VERSION = 7

TABLE = {
    "help": CLI_CMD_HELP,
    "status": CLI_CMD_STATUS,
    "tasks": CLI_CMD_TASKS,
    "can": CLI_CMD_CAN,
    "touch": CLI_CMD_TOUCH,
    "version": CLI_CMD_VERSION,
}


def cli_parse_line(line: str) -> tuple[int, int, list[str], int]:
    tokens: list[str] = []
    too_many = 0
    parts = []
    current = []
    for ch in line:
        if ch in (" ", "\t"):
            if current:
                parts.append("".join(current))
                current = []
        else:
            current.append(ch)
    if current:
        parts.append("".join(current))

    for token in parts:
        if len(tokens) < CLI_PARSE_MAX_TOKENS:
            tokens.append(token)
        else:
            too_many = 1

    if not tokens:
        return CLI_CMD_EMPTY, 0, [], 0

    cmd_id = TABLE.get(tokens[0], CLI_CMD_UNKNOWN)
    argc = len(tokens) - 1
    if too_many:
        argc = CLI_PARSE_MAX_ARGS
    return cmd_id, argc, tokens, too_many


def expect(name: str, got, want) -> None:
    if got != want:
        raise SystemExit(f"FAIL {name}: got {got!r} want {want!r}")


def main() -> None:
    cmd_id, argc, tokens, too_many = cli_parse_line("")
    expect("empty id", cmd_id, CLI_CMD_EMPTY)
    expect("empty argc", argc, 0)

    cmd_id, argc, tokens, too_many = cli_parse_line("   \t  ")
    expect("spaces id", cmd_id, CLI_CMD_EMPTY)

    cmd_id, argc, tokens, too_many = cli_parse_line("help")
    expect("help id", cmd_id, CLI_CMD_HELP)
    expect("help argc", argc, 0)
    expect("help argv0", tokens[0], "help")

    cmd_id, argc, tokens, too_many = cli_parse_line("  status  ")
    expect("status id", cmd_id, CLI_CMD_STATUS)

    cmd_id, argc, tokens, too_many = cli_parse_line("tasks a b")
    expect("tasks id", cmd_id, CLI_CMD_TASKS)
    expect("tasks argc", argc, 2)
    expect("tasks arg1", tokens[1], "a")
    expect("tasks arg2", tokens[2], "b")

    for name, want in (
        ("can", CLI_CMD_CAN),
        ("touch", CLI_CMD_TOUCH),
        ("version", CLI_CMD_VERSION),
    ):
        cmd_id, argc, tokens, too_many = cli_parse_line(name)
        expect(name, cmd_id, want)

    cmd_id, argc, tokens, too_many = cli_parse_line("HELP")
    expect("HELP unknown", cmd_id, CLI_CMD_UNKNOWN)
    expect("HELP argv0", tokens[0], "HELP")

    cmd_id, argc, tokens, too_many = cli_parse_line("foo")
    expect("foo unknown", cmd_id, CLI_CMD_UNKNOWN)

    cmd_id, argc, tokens, too_many = cli_parse_line("help 1 2 3 4 5 6")
    expect("six argc", argc, 6)
    expect("six too_many", too_many, 0)
    expect("six last", tokens[6], "6")

    cmd_id, argc, tokens, too_many = cli_parse_line("help 1 2 3 4 5 6 7")
    expect("seven too_many", too_many, 1)
    expect("seven argc cap", argc, 6)
    expect("seven still help", cmd_id, CLI_CMD_HELP)

    print("cli_parse tests OK")


if __name__ == "__main__":
    main()
