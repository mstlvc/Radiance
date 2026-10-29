<#
    Builds Radiance and drops a single runnable Radiance.exe into .\dist

    Usage (from the repo root):
        .\build.ps1              single-file, self-contained exe in .\dist
        .\build.ps1 -Framework   small exe, needs the .NET 8 runtime installed
#>
param(
    [switch]$Framework
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$project = Join-Path $root 'src\Radiance\Radiance.csproj'
$dist = Join-Path $root 'dist'

Write-Host 'Building...' -ForegroundColor Cyan

if ($Framework) {
    dotnet publish $project -c Release -r win-x64 --self-contained false `
        -p:PublishSingleFile=true -p:DebugType=none
    $from = Join-Path $root 'src\Radiance\bin\Release\net8.0-windows\win-x64\publish'
}
else {
    dotnet publish $project -c Release -r win-x64 --self-contained true `
        -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true `
        -p:EnableCompressionInSingleFile=false -p:DebugType=none
    $from = Join-Path $root 'src\Radiance\bin\Release\net8.0-windows\win-x64\publish'
}

if ($LASTEXITCODE -ne 0) {
    Write-Host 'Build failed.' -ForegroundColor Red
    exit $LASTEXITCODE
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
Remove-Item (Join-Path $dist 'Radiance.exe') -ErrorAction SilentlyContinue
Copy-Item (Join-Path $from 'Radiance.exe') $dist

$exe = Get-Item (Join-Path $dist 'Radiance.exe')
Write-Host ''
Write-Host ("Done: {0}" -f $exe.FullName) -ForegroundColor Green
Write-Host ("Size: {0:N0} MB" -f ($exe.Length / 1MB))