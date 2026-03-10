# Скрипт-обертка для cmake configure с автоматическим обновлением compile_commands.json
param(
    [Parameter(Mandatory=$true)]
    [string]$PresetName
)

# Запускаем cmake configure
Write-Host "Running CMake configure with preset: $PresetName" -ForegroundColor Cyan
cmake --preset $PresetName

if ($LASTEXITCODE -eq 0) {
    # Определяем путь к compile_commands.json в директории сборки
    $buildDir = "out/build/$PresetName"
    $buildCompileCommands = Join-Path $buildDir "compile_commands.json"
    $rootCompileCommands = "compile_commands.json"
    $scriptPath = ".vscode/add_headers_to_compile_commands.py"
    
    # Проверяем, существует ли файл
    if (Test-Path $buildCompileCommands) {
        Write-Host "`nUpdating compile_commands.json with header files..." -ForegroundColor Cyan
        python $scriptPath $buildCompileCommands $rootCompileCommands
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Successfully updated compile_commands.json" -ForegroundColor Green
        } else {
            Write-Host "Warning: Failed to update compile_commands.json" -ForegroundColor Yellow
        }
    } else {
        Write-Host "Warning: compile_commands.json not found at $buildCompileCommands" -ForegroundColor Yellow
    }
} else {
    Write-Host "CMake configure failed" -ForegroundColor Red
    exit $LASTEXITCODE
}

