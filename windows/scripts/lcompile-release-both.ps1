[CmdletBinding()]
param([string]$Target = 'ArongStudyImeTsf', [switch]$Reconfigure)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'lcompile-release.ps1') 32 -Target $Target -Reconfigure:$Reconfigure
& (Join-Path $PSScriptRoot 'lcompile-release.ps1') 64 -Target $Target -Reconfigure:$Reconfigure
