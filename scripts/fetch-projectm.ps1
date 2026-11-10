# Optional local MilkDrop renderer. No presets or NestDrop binaries are downloaded.
$ErrorActionPreference = 'Stop'
$workspacePath = Split-Path -Parent $PSScriptRoot
$dependencyRoot = Join-Path $workspacePath '.local/deps'
$projectmPrefix = Join-Path $dependencyRoot 'projectm'
$ownedProcess = [System.Diagnostics.Process]::GetCurrentProcess()
$oldPriority = $ownedProcess.PriorityClass
$oldCompiler = $env:_CL_
$oldWorkers = $env:CMAKE_BUILD_PARALLEL_LEVEL
function Get-PinnedArchive([string]$url, [string]$path, [string]$hash) {
    if (-not (Test-Path -LiteralPath $path)) { Invoke-WebRequest $url -OutFile $path }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $hash) { throw "Dependency archive identity mismatch: $path" }
}
try {
    $ownedProcess.PriorityClass = 'BelowNormal'
    $env:_CL_ = ($oldCompiler + ' /MP1').Trim()
    $env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
    New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
    $projectmArchive = Join-Path $dependencyRoot 'libprojectM-4.1.8.tar.gz'
    Get-PinnedArchive 'https://github.com/projectM-visualizer/projectm/releases/download/v4.1.8/libprojectM-4.1.8.tar.gz' $projectmArchive 'f72135e8b8e5fd807f908537255b41ac7ba4830309265e830cb756535487239e'
    $glewArchive = Join-Path $dependencyRoot 'glew-2.3.1-win32.zip'
    Get-PinnedArchive 'https://github.com/nigels-com/glew/releases/download/glew-2.3.1/glew-2.3.1-win32.zip' $glewArchive '3792b7bb563d2870ce08491fd3f1147f2fe6a27a43076df475eea7c2b853fd0f'
    $sourcePath = Join-Path $dependencyRoot 'libprojectM-4.1.8'
    $glewPath = Join-Path $dependencyRoot 'glew-2.3.1'
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        tar -xf $projectmArchive -C $dependencyRoot
        if ($LASTEXITCODE -ne 0) { throw 'projectM extraction failed.' }
    }
    $projectmPatches = @('projectm-nested-input.patch', 'projectm-live-controls.patch')
    $projectmSource = '.local/deps/libprojectM-4.1.8'
    $latestPatch = Join-Path $PSScriptRoot $projectmPatches[-1]
    # The newer patch changes lines introduced by the first patch.
    git -C $workspacePath apply --reverse --check "--directory=$projectmSource" $latestPatch 2>$null
    if ($LASTEXITCODE -ne 0) {
        foreach ($patchName in $projectmPatches) {
            $patchPath = Join-Path $PSScriptRoot $patchName
            git -C $workspacePath apply --reverse --check "--directory=$projectmSource" $patchPath 2>$null
            if ($LASTEXITCODE -ne 0) {
                git -C $workspacePath apply "--directory=$projectmSource" $patchPath
                if ($LASTEXITCODE -ne 0) { throw "projectM patch failed: $patchName" }
            }
        }
    }
    if (-not (Test-Path -LiteralPath $glewPath)) { Expand-Archive -LiteralPath $glewArchive -DestinationPath $dependencyRoot }
    $buildPath = Join-Path $dependencyRoot 'projectm-build'
    cmake -S $sourcePath -B $buildPath -G 'Visual Studio 17 2022' -A x64 '-DBUILD_SHARED_LIBS=ON' '-DENABLE_PLAYLIST=OFF' '-DENABLE_SYSTEM_PROJECTM_EVAL=OFF' '-DBUILD_TESTING=OFF' '-DBUILD_DOCS=OFF' "-DCMAKE_INSTALL_PREFIX=$projectmPrefix" "-DGLEW_ROOT=$glewPath"
    if ($LASTEXITCODE -ne 0) { throw 'projectM configuration failed.' }
    cmake --build $buildPath --config Release --parallel 1 --target install
    if ($LASTEXITCODE -ne 0) { throw 'projectM build failed.' }
    Copy-Item -LiteralPath (Join-Path $glewPath 'bin/Release/x64/glew32.dll') -Destination (Join-Path $projectmPrefix 'bin/glew32.dll')
    $licenses = Join-Path $projectmPrefix 'licenses'
    New-Item -ItemType Directory -Path $licenses -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourcePath 'LICENSE.txt') -Destination (Join-Path $licenses 'projectM-LGPL-2.1.txt')
    Copy-Item -LiteralPath (Join-Path $sourcePath 'COPYING') -Destination (Join-Path $licenses 'projectM-COPYING.txt')
    Copy-Item -LiteralPath (Join-Path $sourcePath 'AUTHORS.txt') -Destination (Join-Path $licenses 'projectM-AUTHORS.txt')
    Copy-Item -LiteralPath (Join-Path $glewPath 'LICENSE.txt') -Destination (Join-Path $licenses 'GLEW.txt')
    foreach ($patchName in $projectmPatches) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $patchName) -Destination $licenses
    }
    foreach ($vendor in @('glm', 'hlslparser', 'projectm-eval', 'SOIL2')) {
        $files = Get-ChildItem -LiteralPath (Join-Path $sourcePath "vendor/$vendor") -File -Recurse | Where-Object { $_.Name -match '^(LICENSE|COPYING|UNLICENSE)' }
        $index = 0
        foreach ($file in $files) { Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $licenses "$vendor-$index-$($file.Name)"); $index++ }
    }
    # The release archive keeps SOIL2/stb/ETC1 notices inline in its C headers.
    Copy-Item -LiteralPath (Join-Path $sourcePath 'vendor/SOIL2') -Destination (Join-Path $licenses 'SOIL2-source') -Recurse -Force
    @{
        version = '4.1.8'; source = 'https://github.com/projectM-visualizer/projectm/releases/tag/v4.1.8'
        archiveSha256 = 'f72135e8b8e5fd807f908537255b41ac7ba4830309265e830cb756535487239e'
        dllSha256 = (Get-FileHash -LiteralPath (Join-Path $projectmPrefix 'bin/projectM-4.dll')).Hash.ToLower()
        glewSha256 = (Get-FileHash -LiteralPath (Join-Path $projectmPrefix 'bin/glew32.dll')).Hash.ToLower()
        modifications = ($projectmPatches -join ', '); presetsBundled = $false
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $licenses 'dependency.json') -Encoding UTF8
    Write-Host "Optional projectM renderer ready: $projectmPrefix"
} finally {
    $env:_CL_ = $oldCompiler
    $env:CMAKE_BUILD_PARALLEL_LEVEL = $oldWorkers
    $ownedProcess.PriorityClass = $oldPriority
    $ownedProcess.Dispose()
}
