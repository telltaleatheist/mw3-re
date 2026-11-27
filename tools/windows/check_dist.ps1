# Checks a package folder (tools\windows\package.cmd): every DLL that its
# programs and DLLs import is in the folder or part of Windows (System32, or
# an api-ms-win-* set), the Visual C++ runtime counting as missing unless it
# is in the folder (a player may not have it); and no game file is in it.
# Needs dumpbin (vcvars64). Usage: check_dist.ps1 <folder>
# The launcher's package: mw3launcher.exe and no game program (each game is its own download, docs/launcher.md,
# Games on demand).
param([Parameter(Mandatory = $true)][string]$Dist)
$ErrorActionPreference = 'Stop'
$sys = Join-Path $env:SystemRoot 'System32'
$runtime = '^(vcruntime|msvcp|concrt|vccorlib)\d'
$missing = @()
$folders = @($Dist)
foreach ($folder in $folders) {
  if (-not (Test-Path -LiteralPath $folder)) { continue }
  $here = Get-ChildItem -LiteralPath $folder -File | ForEach-Object { $_.Name.ToLowerInvariant() }
  foreach ($bin in Get-ChildItem -LiteralPath $folder -File | Where-Object { $_.Extension -in '.exe', '.dll' }) {
    $deps = & dumpbin /nologo /dependents $bin.FullName | Where-Object { $_ -match '^\s+(\S+\.dll)\s*$' } |
      ForEach-Object { $Matches[1].ToLowerInvariant() }
    foreach ($d in $deps) {
      if ($here -contains $d) { continue }
      if ($d -match '^(api|ext)-ms-win-') { continue }
      if ($d -notmatch $runtime -and (Test-Path -LiteralPath (Join-Path $sys $d))) { continue }
      $missing += "$($bin.Name) needs $d"
    }
  }
}
# Our free fonts are the only .ttf a package may hold (assets\launcher); the game's are not.
$ours = 'font.ttf', 'font_body.ttf'
# The launcher's own menu sounds (generated: assets\launcher\README.md), the only .wav a package may hold.
$ourWavs = (Join-Path (Resolve-Path -LiteralPath $Dist).Path 'assets\launcher\click.wav'), (Join-Path (Resolve-Path -LiteralPath $Dist).Path 'assets\launcher\hover.wav')
# By the full path: package.cmd passes the folder relative, the files come back absolute.
$game = Get-ChildItem -LiteralPath $Dist -Recurse -File |
  Where-Object { $_.FullName -notin $ourWavs } |
  Where-Object { $_.Extension -in '.zbd', '.avi', '.fmv', '.m1v', '.zrd', '.wav', '.ogg', '.flac' -or $_.Name -ieq 'mech3.exe' -or
                 ($_.Extension -ieq '.ttf' -and $_.Name -cnotin $ours) }
# Where the launcher looks (docs\launcher.md): without it, only the built-in copy.
if (-not (Test-Path -LiteralPath (Join-Path $Dist 'assets\launcher\sources.json'))) { $missing += 'assets\launcher\sources.json' }
if (-not (Test-Path -LiteralPath (Join-Path $Dist 'assets\launcher\launcher-settings.json'))) { $missing += 'assets\launcher\launcher-settings.json' }
# The launcher (the main shortcut) and no game program.
foreach ($exe in 'mw3.exe', 'mw3pm.exe', 'mw1.exe', 'mw3import.exe', 'mw3indeo.exe') {
  if (Test-Path -LiteralPath (Join-Path $Dist $exe)) { $missing += "$exe should not be in the launcher's package" }
}
# FFmpeg is the games' (their movie importer): the launcher loads none of it.
Get-ChildItem -LiteralPath $Dist -File -Filter '*.dll' | Where-Object { $_.Name -like 'av*.dll' -or $_.Name -like 'sw*.dll' } |
  ForEach-Object { $missing += "$($_.Name) (FFmpeg's) should not be in the launcher's package" }
if (-not (Test-Path -LiteralPath (Join-Path $Dist 'mw3launcher.exe'))) { $missing += "mw3launcher.exe (the program itself)" }
# The libraries linked into mw3launcher.exe carry no file of their own: their licences must be there (FreeType's,
# RmlUi's font engine, from build_deps_ttf.cmd's prefix).
foreach ($lib in 'libarchive-*', 'zlib-*', 'xz-*', 'rmlui', 'miniz', 'monocypher', 'freetype-*') {
  $dir = Get-ChildItem -LiteralPath (Join-Path $Dist 'licenses') -Directory -Filter $lib -ErrorAction SilentlyContinue
  if (-not $dir -or -not (Get-ChildItem -LiteralPath $dir[0].FullName -File)) { $missing += "licenses\$lib (its licence)" }
}
# No local path or user name in any file (release.md, Privacy; tools/deps/check_paths.py
# on the Mac and Linux): every byte, as ASCII and UTF-16. The public release addresses are allowed:
# github.com/<user>, the updater's api.github.com/repos/<user>, and the backup's (2026-11-22)
# git.owenmorgan.com/<user> and .../api/v1/repos/<user>, its host name included, and the games catalog's
# raw.githubusercontent.com/<user> (2026-11-26, docs/launcher-split.md, b7).
$words = @('/Volumes/', '/Users/', '/home/', 'C:\Users\', 'C:/Users/') +
  @($env:USERNAME, (Split-Path -Leaf $env:USERPROFILE)) | Where-Object { $_ -and $_.Length -ge 4 } | Select-Object -Unique
$rx = [regex](($words | ForEach-Object { [regex]::Escape($_) }) -join '|')
$latin = [Text.Encoding]::GetEncoding(28591)
$public = @('github.com/', 'github.com/repos/', 'raw.githubusercontent.com/', 'git.owenmorgan.com/',
  'git.owenmorgan.com/api/v1/repos/')
$host_ = 'git.owenmorgan.com'
function Test-Public($text, $m) {
  foreach ($p in $public) {
    if ($m.Index -ge $p.Length -and $text.Substring($m.Index - $p.Length, $p.Length) -ieq $p) { return $true }
  }
  # the name inside the backup's own host name
  $from = [Math]::Max(0, $m.Index - $host_.Length)
  $at = $text.IndexOf($host_, $from, [Math]::Min($text.Length - $from, 2 * $host_.Length), [StringComparison]::OrdinalIgnoreCase)
  return ($at -ge 0 -and $at -le $m.Index -and $m.Index + $m.Length -le $at + $host_.Length)
}
$leaks = @()
foreach ($f in Get-ChildItem -LiteralPath $Dist -Recurse -File) {
  $bytes = [IO.File]::ReadAllBytes($f.FullName)
  foreach ($text in $latin.GetString($bytes), [Text.Encoding]::Unicode.GetString($bytes)) {
    $hit = $rx.Matches($text) | Where-Object { -not (Test-Public $text $_) } | Select-Object -First 1
    if ($hit) { $leaks += "$($f.FullName.Substring($Dist.Length)): $($hit.Value)"; break }
  }
}
if ($leaks) { $leaks | ForEach-Object { Write-Host "local path or user name in the package: $_" } }
if ($missing.Count -gt 0) { $missing | ForEach-Object { Write-Host "missing: $_" } }
if ($game) { $game | ForEach-Object { Write-Host "game file in the package: $($_.FullName)" } }
if ($missing.Count -gt 0 -or $game -or $leaks) { exit 1 }
$n = (Get-ChildItem -LiteralPath $Dist -File).Count
Write-Host "check_dist: $n files; every DLL present or Windows'; the linked-in libraries' licences; no game files; no local path or user name"
