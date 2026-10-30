# The libraries built into the package's FFmpeg DLLs (tools\windows\package.cmd):
# read from FFmpeg's configure line (licenses\ffmpeg-configuration.txt, from
# mw3import --version) and written with their licences to
# licenses\ffmpeg-libraries.txt. A library this table does not know is listed
# for a person to check, and the script says so. Usage: ffmpeg_notices.ps1 <dist>
param([Parameter(Mandatory = $true)][string]$Dist)
$ErrorActionPreference = 'Stop'
$conf = Get-Content -LiteralPath (Join-Path $Dist 'licenses\ffmpeg-configuration.txt') -Raw
$known = @{
  'libaom'        = 'BSD-2-Clause (AOMedia), https://aomedia.googlesource.com/aom'
  'libdav1d'      = 'BSD-2-Clause, https://code.videolan.org/videolan/dav1d'
  'libvpx'        = 'BSD-3-Clause, https://chromium.googlesource.com/webm/libvpx'
  'libopus'       = 'BSD-3-Clause, https://opus-codec.org/'
  'libvorbis'     = 'BSD-3-Clause (with libogg, BSD-3-Clause), https://xiph.org/vorbis/'
  'libtheora'     = 'BSD-3-Clause, https://theora.org/'
  'libspeex'      = 'BSD-3-Clause, https://speex.org/'
  'libmp3lame'    = 'LGPL-2.0-or-later, https://lame.sourceforge.io/'
  'libopencore-amrnb' = 'Apache-2.0, https://sourceforge.net/projects/opencore-amr/'
  'libopencore-amrwb' = 'Apache-2.0, https://sourceforge.net/projects/opencore-amr/'
  'libopenh264'   = 'BSD-2-Clause, https://github.com/cisco/openh264'
  'librav1e'      = 'BSD-2-Clause, https://github.com/xiph/rav1e'
  'libzimg'       = 'WTFPL, https://github.com/sekrit-twc/zimg'
  'libxml2'       = 'MIT, https://gitlab.gnome.org/GNOME/libxml2'
  'libbluray'     = 'LGPL-2.1-or-later, https://www.videolan.org/developers/libbluray.html'
  'libwebp'       = 'BSD-3-Clause, https://chromium.googlesource.com/webm/libwebp'
  'libjxl'        = 'BSD-3-Clause, https://github.com/libjxl/libjxl'
  'libass'        = 'ISC, https://github.com/libass/libass'
  'libfreetype'   = 'FreeType License (FTL), https://freetype.org/license.html'
  'libharfbuzz'   = 'MIT ("Old MIT"), https://github.com/harfbuzz/harfbuzz'
  'libfribidi'    = 'LGPL-2.1-or-later, https://github.com/fribidi/fribidi'
  'libopenjpeg'   = 'BSD-2-Clause, https://www.openjpeg.org/'
  'libsrt'        = 'MPL-2.0, https://github.com/Haivision/srt'
  'libopenmpt'    = 'BSD-3-Clause, https://lib.openmpt.org/'
  'libsoxr'       = 'LGPL-2.1-or-later, https://sourceforge.net/projects/soxr/'
  'libtwolame'    = 'LGPL-2.1-or-later, https://www.twolame.org/'
  'libvpl'        = 'MIT, https://github.com/intel/libvpl'
  'libshaderc'    = 'Apache-2.0, https://github.com/google/shaderc'
  'libplacebo'    = 'LGPL-2.1-or-later, https://code.videolan.org/videolan/libplacebo'
  'libgme'        = 'LGPL-2.1-or-later, https://github.com/libgme/game-music-emu'
  'libkvazaar'    = 'BSD-3-Clause, https://github.com/ultravideo/kvazaar'
  'libssh'        = 'LGPL-2.1, https://www.libssh.org/'
  'libzmq'        = 'MPL-2.0, https://zeromq.org/'
  'iconv'         = 'LGPL-2.1-or-later (GNU libiconv), https://www.gnu.org/software/libiconv/'
  'zlib'          = 'Zlib, https://zlib.net/'
  'bzlib'         = 'bzip2 licence (BSD-style), https://sourceware.org/bzip2/'
  'lzma'          = 'public domain / 0BSD (XZ Utils), https://tukaani.org/xz/'
  'gmp'           = 'LGPL-3.0-or-later, https://gmplib.org/'
  'openssl'       = 'Apache-2.0, https://www.openssl.org/'
}
$libs = [regex]::Matches($conf, '--enable-((?:lib[\w-]+)|iconv|zlib|bzlib|lzma|gmp|openssl)\b') |
  ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
$v3 = $conf -match '--enable-version3'
$lines = @("FFmpeg's DLLs in this package: LGPL $(if ($v3) { 'version 3' } else { 'version 2.1' }) or later",
           "(licenses\ffmpeg-configuration.txt: the configure line). Built in, each under its own licence:", '')
$unknown = @()
foreach ($l in $libs) {
  if ($known.ContainsKey($l)) { $lines += "  $l : $($known[$l])" }
  else { $lines += "  $l : (check its project's licence)"; $unknown += $l }
}
Set-Content -LiteralPath (Join-Path $Dist 'licenses\ffmpeg-libraries.txt') -Value $lines -Encoding ascii
if ($unknown.Count -gt 0) { Write-Host "ffmpeg_notices: licence to check by hand: $($unknown -join ', ')" }
Write-Host "ffmpeg_notices: $($libs.Count) libraries listed, LGPL $(if ($v3) { 'v3' } else { 'v2.1' })"
