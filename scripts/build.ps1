param([switch]$TestPlugin, [string]$VerificationManifest)
$ErrorActionPreference = 'Stop'
$workspacePath = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $workspacePath
try {
    & (Join-Path $PSScriptRoot 'fetch-analysis.ps1')
    $dependencyPath = Join-Path $workspacePath '.local\deps\JUCE'
    $expectedCommit = 'ea77444750de45e036e0279bb0291144bf8e3688'
    if (-not (Test-Path -LiteralPath (Join-Path $dependencyPath 'CMakeLists.txt'))) {
        git clone --depth 1 --branch 8.0.16-security https://github.com/juce-framework/JUCE.git $dependencyPath
        if ($LASTEXITCODE -ne 0) { throw 'JUCE download failed.' }
    }
    $actualCommit = git -C $dependencyPath rev-parse HEAD
    if ($actualCommit -ne $expectedCommit) { throw "Expected JUCE $expectedCommit, found $actualCommit. Existing dependency was kept." }
    $stretchPath = Join-Path $workspacePath '.local\deps\soundtouch'
    $stretchCommit = 'd994965fbbcf0f6ceeed0e72516968130c2912f0'
    if (-not (Test-Path -LiteralPath (Join-Path $stretchPath 'CMakeLists.txt'))) {
        git -c http.sslBackend=openssl clone --depth 1 --branch 2.4.0 https://codeberg.org/soundtouch/soundtouch.git $stretchPath
        if ($LASTEXITCODE -ne 0) { throw 'SoundTouch dependency download failed.' }
    }
    if ((git -C $stretchPath rev-parse HEAD) -ne $stretchCommit) { throw 'SoundTouch dependency identity mismatch. Existing source was kept.' }
    $testOption = if ($TestPlugin) { 'ON' } else { 'OFF' }
    cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 "-DMIXWORKS_BUILD_TEST_PLUGIN=$testOption"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    $targets = @('Mixworks')
    if ($TestPlugin) { $targets += 'MixworksTestGain_VST3', 'MixworksTestDelay_VST3', 'MixworksTestLatency_VST3' }
    cmake --build build --config Release --target @targets --parallel 3
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    if ($VerificationManifest) { & (Join-Path $PSScriptRoot 'deploy.ps1') -VerificationManifest $VerificationManifest }
    else { Write-Host 'Build ready. Verify this executable, then deploy.ps1 with its VerificationManifest. The launcher was kept.' }
} finally { Pop-Location }
