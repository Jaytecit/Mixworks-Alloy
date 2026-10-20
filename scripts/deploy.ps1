param([Parameter(Mandatory = $true)][string]$VerificationManifest, [switch]$VerifyOnly)
$ErrorActionPreference = 'Stop'
$workspacePath = Split-Path -Parent $PSScriptRoot
$builtApp = (Get-Content -LiteralPath (Join-Path $workspacePath 'build\mixworks-Release.path') -Raw).Trim()
$releaseVersion = (Get-Item -LiteralPath $builtApp).VersionInfo.ProductVersion
if ($releaseVersion -notmatch '^\d+\.\d+\.\d+$') { throw 'The built executable has no valid release version.' }
$releaseHash = (Get-FileHash -LiteralPath $builtApp -Algorithm SHA256).Hash
$pins = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'analysis-dependencies.json') -Raw | ConvertFrom-Json
$verified = Get-Content -LiteralPath $VerificationManifest -Raw | ConvertFrom-Json
& python (Join-Path $PSScriptRoot 'verification.py') --receipt $VerificationManifest --exe $builtApp
if ($LASTEXITCODE -ne 0) { throw 'Complete candidate verification is required. The launcher was kept.' }
if ($verified.executableSha256 -ne $releaseHash -or
    $verified.modelSha256 -ne $pins.beatThisCpp.modelSha256 -or $verified.runtimeSha256 -ne $pins.onnxRuntime.dllSha256 -or
    $verified.scope -ne 'complete-regressions') { throw 'Verification does not establish complete regressions for this exact executable and analysis bundle. The launcher was kept.' }
$bundle = @(
    @{name='onnxruntime.dll'; path=(Join-Path (Split-Path -Parent $builtApp) 'onnxruntime.dll'); hash=$pins.onnxRuntime.dllSha256},
    @{name='beat-this-final0.onnx'; path=(Join-Path (Split-Path -Parent $builtApp) 'beat-this-final0.onnx'); hash=$pins.beatThisCpp.modelSha256},
    @{name='THIRD-PARTY-NOTICES.md'; path=(Join-Path $workspacePath 'THIRD-PARTY-NOTICES.md'); hash=$null},
    @{name='LICENSE'; path=(Join-Path $workspacePath 'LICENSE'); hash=$null},
    @{name='docs/LICENSING.md'; path=(Join-Path $workspacePath 'docs/LICENSING.md'); hash=$null}
)
foreach ($item in $bundle) {
    $actual = (Get-FileHash -LiteralPath $item.path -Algorithm SHA256).Hash
    if ($item.hash -and $actual -ne $item.hash) { throw "Analysis bundle identity mismatch: $($item.name). The launcher was kept." }
    $item.hash = $actual
}
if ($VerifyOnly) { Write-Host 'Verification accepted. No release files or launcher were changed.'; return }
$releaseName = $releaseVersion + '-' + $releaseHash.Substring(0, 12).ToLowerInvariant()
$releasePath = Join-Path $workspacePath ('out\releases\' + $releaseName)
New-Item -ItemType Directory -Path $releasePath -Force | Out-Null
$deployedApp = Join-Path $releasePath 'Mixworks.exe'
if (-not (Test-Path -LiteralPath $deployedApp)) { Copy-Item -LiteralPath $builtApp -Destination $deployedApp }
if ((Get-FileHash -LiteralPath $deployedApp -Algorithm SHA256).Hash -ne $releaseHash) { throw 'Release identity mismatch; the existing executable was kept.' }
foreach ($item in $bundle) {
    $destination = Join-Path $releasePath $item.name
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    if (-not (Test-Path -LiteralPath $destination)) { Copy-Item -LiteralPath $item.path -Destination $destination }
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $item.hash) { throw "An existing release asset differs and was kept: $($item.name)" }
}
$licensePath = Join-Path $releasePath 'licenses'
New-Item -ItemType Directory -Path $licensePath -Force | Out-Null
foreach ($license in Get-ChildItem -LiteralPath (Join-Path $workspacePath 'licenses') -File) {
    $destination = Join-Path $licensePath $license.Name
    if (-not (Test-Path -LiteralPath $destination)) { Copy-Item -LiteralPath $license.FullName -Destination $destination }
    if ((Get-FileHash -LiteralPath $destination).Hash -ne (Get-FileHash -LiteralPath $license.FullName).Hash) { throw 'An existing release license differs and was kept.' }
}
$receiptName = 'verification-' + (Get-FileHash -LiteralPath $VerificationManifest).Hash.Substring(0, 16).ToLowerInvariant() + '.json'
$receiptPath = Join-Path $releasePath $receiptName
if (-not (Test-Path -LiteralPath $receiptPath)) { Copy-Item -LiteralPath $VerificationManifest -Destination $receiptPath }
if ((Get-FileHash -LiteralPath $receiptPath).Hash -ne (Get-FileHash -LiteralPath $VerificationManifest).Hash) { throw 'An existing verification receipt differs and was kept.' }
# Each release owns an immutable executable, so updating the launcher never replaces a running app.
$launcher = Join-Path $workspacePath 'out\Launch.cmd'
$launcherText = '@echo off' + "`r`n" + 'start "" "%~dp0releases\' + $releaseName + '\Mixworks.exe"' + "`r`n"
if (-not (Test-Path -LiteralPath $launcher) -or (Get-Content -LiteralPath $launcher -Raw) -ne $launcherText) {
    if (Test-Path -LiteralPath $launcher) {
        $launcherBackup = $launcher + '.bak'
        $launcherBackupIndex = 1
        while (Test-Path -LiteralPath $launcherBackup) { $launcherBackup = $launcher + '.bak.' + $launcherBackupIndex; $launcherBackupIndex++ }
        Copy-Item -LiteralPath $launcher -Destination $launcherBackup
    }
    Set-Content -LiteralPath $launcher -Value $launcherText -Encoding Ascii -NoNewline
}
Write-Host "Ready: $deployedApp"
