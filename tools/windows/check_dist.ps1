# Checks a package folder (tools\windows\package.cmd): every DLL that its
# programs and DLLs import is in the folder or part of Windows (System32, or
# an api-ms-win-* set), the Visual C++ runtime counting as missing unless it
# is in the folder (a player may not have it); and no game file is in it.
# Needs dumpbin (vcvars64). Usage: check_dist.ps1 <folder>
param([Parameter(Mandatory = $true)][string]$Dist)
$ErrorActionPreference = 'Stop'
$here = Get-ChildItem -LiteralPath $Dist -File | ForEach-Object { $_.Name.ToLowerInvariant() }
$sys = Join-Path $env:SystemRoot 'System32'
$runtime = '^(vcruntime|msvcp|concrt|vccorlib)\d'
$missing = @()
foreach ($bin in Get-ChildItem -LiteralPath $Dist -File | Where-Object { $_.Extension -in '.exe', '.dll' }) {
  $deps = & dumpbin /nologo /dependents $bin.FullName | Where-Object { $_ -match '^\s+(\S+\.dll)\s*$' } |
    ForEach-Object { $Matches[1].ToLowerInvariant() }
  foreach ($d in $deps) {
    if ($here -contains $d) { continue }
    if ($d -match '^(api|ext)-ms-win-') { continue }
    if ($d -notmatch $runtime -and (Test-Path -LiteralPath (Join-Path $sys $d))) { continue }
    $missing += "$($bin.Name) needs $d"
  }
}
$game = Get-ChildItem -LiteralPath $Dist -Recurse -File |
  Where-Object { $_.Extension -in '.zbd', '.avi', '.fmv', '.m1v', '.ttf', '.zrd', '.wav', '.ogg', '.flac' -or $_.Name -ieq 'mech3.exe' }
if ($missing.Count -gt 0) { $missing | ForEach-Object { Write-Host "missing: $_" } }
if ($game) { $game | ForEach-Object { Write-Host "game file in the package: $($_.FullName)" } }
if ($missing.Count -gt 0 -or $game) { exit 1 }
$n = (Get-ChildItem -LiteralPath $Dist -File).Count
Write-Host "check_dist: $n files; every DLL present or Windows'; no game files"
