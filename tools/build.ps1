# BOOM 工程一键构建脚本 (PowerShell)
# 用法: pwsh -File build.ps1  或  powershell -ExecutionPolicy Bypass -File build.ps1
$ErrorActionPreference = 'Stop'
$proj = 'D:\ESP32\Project\END\Boom\BOOM'
$idf  = 'D:\ESP32\Espressif\frameworks\esp-idf-v5.4.4'
$env:IDF_PATH = $idf
$env:IDF_TOOLS_PATH = 'D:\ESP32\Espressif'
$env:IDF_PYTHON_ENV_PATH = 'D:\ESP32\Espressif\python_env\idf5.4_py3.11_env'
$env:PATH = ( @(
    'D:\ESP32\Espressif\tools\cmake\3.30.2\bin',
    'D:\ESP32\Espressif\tools\ninja\1.12.1',
    'D:\ESP32\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin',
    'D:\ESP32\Espressif\tools\ccache\4.12.1\ccache-4.12.1-windows-x86_64',
    'D:\ESP32\Espressif\python_env\idf5.4_py3.11_env\Scripts'
) -join ';' ) + ';' + $env:PATH
Set-Location -LiteralPath $proj
$py = 'D:\ESP32\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe'
& $py "$idf\tools\idf.py" build
exit $LASTEXITCODE
