[CmdletBinding()]
param(
    [switch]$ViewOnly
)

$ErrorActionPreference = 'Stop'
$config = Join-Path $PSScriptRoot 'simulations.txt'
$simulator = Join-Path $PSScriptRoot 'build/Release/nbody_sim.exe'
$viewer = Join-Path $PSScriptRoot 'build/visualization/Release/nbody_viewer.exe'

if (-not (Test-Path -LiteralPath $config -PathType Leaf)) {
    throw "Batch configuration not found: $config"
}
if (-not (Test-Path -LiteralPath $viewer -PathType Leaf)) {
    throw "3D viewer not found: $viewer. Build Release with NBODY_BUILD_VIEWER=ON."
}

if (-not $ViewOnly) {
    if (-not (Test-Path -LiteralPath $simulator -PathType Leaf)) {
        throw "Simulator not found: $simulator. Build the Release configuration first."
    }
    Push-Location -LiteralPath $PSScriptRoot
    try {
        & $simulator --config $config
        if ($LASTEXITCODE -ne 0) {
            throw "Simulation batch failed (exit code $LASTEXITCODE)."
        }
    } finally {
        Pop-Location
    }
}

$section = ''
$needsName = $false
$seen = @{}
$names = @(foreach ($rawLine in Get-Content -LiteralPath $config -Encoding UTF8) {
    $line = ($rawLine -replace '[#;].*$', '').Trim()
    if ($line -cmatch '^\[.*\]$') {
        if ($needsName) { throw 'A [simulation] section is missing its name.' }
        $section = $line
        $needsName = $section -ceq '[simulation]'
    } elseif ($section -ceq '[simulation]' -and $line -cmatch '^name\s*=(.*)$') {
        $name = $Matches[1].Trim()
        if ($name -cnotmatch '^[A-Za-z0-9_-]+$') {
            throw "Invalid simulation name: '$name'."
        }
        if (-not $needsName -or $seen.ContainsKey($name)) {
            throw "Duplicate simulation name or name setting: '$name'."
        }
        $seen[$name] = $true
        $needsName = $false
        $name
    }
})
if ($needsName) { throw 'A [simulation] section is missing its name.' }
if ($names.Count -eq 0) { throw "No simulations were found in $config." }

$viewed = 0
for ($i = 0; $i -lt $names.Count; $i++) {
    $name = $names[$i]
    $remaining = $names.Count - $i - 1
    $trajectory = Join-Path $PSScriptRoot "output/$name/trajectory.csv"
    if (-not (Test-Path -LiteralPath $trajectory -PathType Leaf)) {
        Write-Warning "Skipping '$name' ($remaining remaining). CSV not found: $trajectory"
        continue
    }

    Write-Host "Viewing '$name' ($($i + 1)/$($names.Count), $remaining remaining). Close the viewer to continue."
    $process = Start-Process -FilePath $viewer -ArgumentList ('"{0}"' -f $trajectory) -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        Write-Warning "Viewer exited with code $($process.ExitCode) for '$name'. Continuing."
    } else {
        $viewed++
    }
}
Write-Host "Playback finished. Viewed $viewed of $($names.Count) simulations."
