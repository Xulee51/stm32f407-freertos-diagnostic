param([Parameter(Mandatory=$true)][ValidateSet('build','clean','flash','probe')][string]$Task)
$ErrorActionPreference='Stop'
$Project=Split-Path -Parent $PSScriptRoot
$Ide='D:\STM32\STM32CubeIDE_1.19.0\STM32CubeIDE'
$Plugins=Join-Path $Ide 'plugins'
function ToolDir([string]$pattern) {
    $hit=Get-ChildItem $Plugins -Directory -Filter $pattern | Sort-Object Name -Descending | Select-Object -First 1
    if(-not $hit){throw "找不到 CubeIDE 工具插件: $pattern"}
    Join-Path $hit.FullName 'tools\bin'
}
$gcc=ToolDir 'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*'
$make=ToolDir 'com.st.stm32cube.ide.mcu.externaltools.make.*'
$prog=ToolDir 'com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.*'
$env:Path="$gcc;$make;$prog;$env:Path"
$elf=Join-Path $Project 'Build\freertos_robot_diagnostic.elf'
switch($Task){
 'build' { & "$make\make.exe" -C $Project -j8 all; if($LASTEXITCODE){throw '编译失败'} }
 'clean' { & "$make\make.exe" -C $Project clean; if($LASTEXITCODE){throw '清理失败'} }
 'probe' { & "$prog\STM32_Programmer_CLI.exe" -l st-link }
 'flash' { if(-not(Test-Path $elf)){throw '请先 build'}; & "$prog\STM32_Programmer_CLI.exe" -c port=SWD mode=UR -w $elf -v -rst; if($LASTEXITCODE){throw '烧录失败'} }
}
