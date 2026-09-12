#!/usr/bin/env bash
# 节点 10：在主机上运行 CLI 解析单元测试。
# 优先用 gcc 编译 C 测试（与固件同源）；本机若无 gcc 则跑 Python 对照用例。
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "${ROOT}/build"
if command -v gcc >/dev/null 2>&1; then
    OUT="${ROOT}/build/cli_parse_test.exe"
    gcc -std=c11 -Wall -Wextra -Werror -I"${ROOT}/Core/Inc" \
        "${ROOT}/tests/cli_parse_test.c" \
        "${ROOT}/Core/Src/cli_parse.c" \
        -o "${OUT}"
    "${OUT}"
else
    python.exe "${ROOT}/tests/cli_parse_test.py"
fi
