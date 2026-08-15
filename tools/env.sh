#!/usr/bin/env bash

# Source this file from Git Bash before configuring or building:
#   source tools/env.sh

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "Please source this script: source tools/env.sh" >&2
    exit 1
fi

set -o pipefail

python_scripts_win="$(python.exe -c 'import sysconfig; print(sysconfig.get_path("scripts", scheme="nt_user"))')"
python_scripts="$(cygpath -u "$python_scripts_win")"

cube_plugins=""
for candidate in /d/STM32/STM32CubeIDE_*/STM32CubeIDE/plugins; do
    if [[ -d "$candidate" ]]; then
        cube_plugins="$candidate"
    fi
done

if [[ -z "$cube_plugins" ]]; then
    echo "STM32CubeIDE plugins directory was not found under D:/STM32." >&2
    return 1
fi

latest_tool_dir() {
    local pattern="$1"
    local match=""
    for candidate in "$cube_plugins"/$pattern; do
        [[ -d "$candidate/tools/bin" ]] && match="$candidate/tools/bin"
    done
    [[ -n "$match" ]] || return 1
    printf '%s\n' "$match"
}

gcc_bin="$(latest_tool_dir 'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*')" || {
    echo "ARM GCC bundled with STM32CubeIDE was not found." >&2
    return 1
}
programmer_bin="$(latest_tool_dir 'com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.*')" || {
    echo "STM32CubeProgrammer bundled with STM32CubeIDE was not found." >&2
    return 1
}
gdb_server_bin="$(latest_tool_dir 'com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server.*')" || {
    echo "ST-LINK GDB server bundled with STM32CubeIDE was not found." >&2
    return 1
}

export ARM_GCC_BIN="$(cygpath -w "$gcc_bin")"
export STM32_PROGRAMMER_BIN="$(cygpath -w "$programmer_bin")"
export STLINK_GDB_SERVER_BIN="$(cygpath -w "$gdb_server_bin")"
export PATH="$python_scripts:$gcc_bin:$programmer_bin:$gdb_server_bin:$PATH"

for tool in cmake ninja arm-none-eabi-gcc STM32_Programmer_CLI.exe; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Required tool not found: $tool" >&2
        return 1
    fi
done

echo "Environment ready:"
echo "  CMake:          $(cmake --version | head -n 1)"
echo "  Ninja:          $(ninja --version)"
echo "  ARM GCC:        $(arm-none-eabi-gcc --version | head -n 1)"
echo "  CubeProgrammer: $(STM32_Programmer_CLI.exe --version 2>&1 | grep -m1 'version:' | sed 's/^[[:space:]]*//')"
