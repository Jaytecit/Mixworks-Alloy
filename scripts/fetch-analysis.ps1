$ErrorActionPreference = 'Stop'
$workspacePath = Split-Path -Parent $PSScriptRoot
$pins = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'analysis-dependencies.json') -Raw | ConvertFrom-Json
$dependencyRoot = Join-Path $workspacePath '.local\deps'
New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
function Assert-Hash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path) -or (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Expected) { throw "Analysis dependency identity mismatch: $Path. Existing bytes were kept." }
}
$modelRoot = Join-Path $dependencyRoot 'beat-this-cpp'
if (-not (Test-Path -LiteralPath $modelRoot)) {
    git init $modelRoot
    if ($LASTEXITCODE -ne 0) { throw 'Beat This source directory could not be created.' }
    git -C $modelRoot -c http.sslBackend=openssl fetch --depth 1 $pins.beatThisCpp.repository $pins.beatThisCpp.commit
    if ($LASTEXITCODE -ne 0) { throw 'Beat This source download failed.' }
    git -C $modelRoot checkout --detach FETCH_HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Beat This source checkout failed.' }
}
if ((git -C $modelRoot rev-parse HEAD) -ne $pins.beatThisCpp.commit) { throw 'The existing Beat This checkout has a different identity and was preserved.' }
git -C $modelRoot diff --quiet
if ($LASTEXITCODE -ne 0) { throw 'The existing Beat This checkout has local changes and was preserved.' }
foreach ($dependency in @(@('pocketfft', $pins.beatThisCpp.pocketfftCommit), @('r8brain', $pins.beatThisCpp.r8brainCommit))) {
    $sourcePath = Join-Path $modelRoot ('Submodule\' + $dependency[0])
    if (Test-Path -LiteralPath (Join-Path $sourcePath '.git')) {
        if ((git -C $sourcePath rev-parse HEAD) -ne $dependency[1]) { throw "An existing analysis dependency differs and was preserved: $sourcePath" }
        git -C $sourcePath diff --quiet
        if ($LASTEXITCODE -ne 0) { throw "An existing analysis dependency has local changes and was preserved: $sourcePath" }
    }
}
git -C $modelRoot -c 'url.https://github.com/.insteadOf=git@github.com:' -c http.sslBackend=openssl submodule update --init Submodule/pocketfft Submodule/r8brain
if ($LASTEXITCODE -ne 0) { throw 'Beat This signal dependencies are unavailable.' }
foreach ($dependency in @(@('pocketfft', $pins.beatThisCpp.pocketfftCommit), @('r8brain', $pins.beatThisCpp.r8brainCommit))) {
    $sourcePath = Join-Path $modelRoot ('Submodule\' + $dependency[0])
    if ((git -C $sourcePath rev-parse HEAD) -ne $dependency[1]) { throw "Unexpected dependency identity: $sourcePath" }
    git -C $sourcePath diff --quiet
    if ($LASTEXITCODE -ne 0) { throw "The existing dependency has local changes and was preserved: $sourcePath" }
}
Assert-Hash (Join-Path $modelRoot $pins.beatThisCpp.model) $pins.beatThisCpp.modelSha256
$runtimeName = 'onnxruntime-win-x64-' + $pins.onnxRuntime.version
$runtimeRoot = Join-Path $dependencyRoot $runtimeName
if (-not (Test-Path -LiteralPath $runtimeRoot)) {
    $archivePath = Join-Path $dependencyRoot ($runtimeName + '.zip')
    if (-not (Test-Path -LiteralPath $archivePath)) { Invoke-WebRequest -Uri $pins.onnxRuntime.url -OutFile $archivePath }
    Assert-Hash $archivePath $pins.onnxRuntime.archiveSha256
    Expand-Archive -LiteralPath $archivePath -DestinationPath $dependencyRoot
}
Assert-Hash (Join-Path $runtimeRoot 'lib\onnxruntime.dll') $pins.onnxRuntime.dllSha256
Write-Host 'Pinned local analysis model and runtime are ready.'
