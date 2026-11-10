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
# Our free fonts are the only .ttf a package may hold (assets\launcher, assets\fonts); the game's are not.
$ours = 'font.ttf', 'LiberationSans-Regular.ttf', 'LiberationMono-Regular.ttf', 'Anton-Regular.ttf'
$game = Get-ChildItem -LiteralPath $Dist -Recurse -File |
  Where-Object { $_.Extension -in '.zbd', '.avi', '.fmv', '.m1v', '.zrd', '.wav', '.ogg', '.flac' -or $_.Name -ieq 'mech3.exe' -or
                 ($_.Extension -ieq '.ttf' -and $_.Name -cnotin $ours) }
# The program (PM's package has no launcher for now).
foreach ($exe in @('mw3pm.exe')) {
  if (-not (Test-Path -LiteralPath (Join-Path $Dist $exe))) { $missing += "$exe (the program itself)" }
}
# The libraries linked into mw3.exe carry no file of their own: their licences must be there.
foreach ($lib in 'libarchive-*', 'zlib-*', 'xz-*', 'imgui') {
  $dir = Get-ChildItem -LiteralPath (Join-Path $Dist 'licenses') -Directory -Filter $lib -ErrorAction SilentlyContinue
  if (-not $dir -or -not (Get-ChildItem -LiteralPath $dir[0].FullName -File)) { $missing += "licenses\$lib (its licence)" }
}
# No local path or user name in any file (release.md, Privacy; tools/deps/check_paths.py
# on the Mac and Linux): every byte, as ASCII and UTF-16; github.com/<user> and the updater's
# api.github.com/repos/<user> are allowed.
$words = @('/Volumes/', '/Users/', '/home/', 'C:\Users\', 'C:/Users/') +
  @($env:USERNAME, (Split-Path -Leaf $env:USERPROFILE)) | Where-Object { $_ -and $_.Length -ge 4 } | Select-Object -Unique
$rx = [regex](($words | ForEach-Object { [regex]::Escape($_) }) -join '|')
$latin = [Text.Encoding]::GetEncoding(28591)
$leaks = @()
foreach ($f in Get-ChildItem -LiteralPath $Dist -Recurse -File) {
  $bytes = [IO.File]::ReadAllBytes($f.FullName)
  foreach ($text in $latin.GetString($bytes), [Text.Encoding]::Unicode.GetString($bytes)) {
    $hit = $rx.Matches($text) | Where-Object { -not (($_.Index -ge 11 -and $text.Substring($_.Index - 11, 11) -ieq 'github.com/') -or
      ($_.Index -ge 17 -and $text.Substring($_.Index - 17, 17) -ieq 'github.com/repos/')) } | Select-Object -First 1
    if ($hit) { $leaks += "$($f.FullName.Substring($Dist.Length)): $($hit.Value)"; break }
  }
}
if ($leaks) { $leaks | ForEach-Object { Write-Host "local path or user name in the package: $_" } }
if ($missing.Count -gt 0) { $missing | ForEach-Object { Write-Host "missing: $_" } }
if ($game) { $game | ForEach-Object { Write-Host "game file in the package: $($_.FullName)" } }
if ($missing.Count -gt 0 -or $game -or $leaks) { exit 1 }
$n = (Get-ChildItem -LiteralPath $Dist -File).Count
Write-Host "check_dist: $n files; every DLL present or Windows'; the linked-in libraries' licences; no game files; no local path or user name"
