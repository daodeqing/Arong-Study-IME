# Confirm the build produced the DLL and PDB the installer collects.
#
# Read the standard VERSIONINFO resource too: Chinese product names require UTF-8 resource input.
param([Parameter(Mandatory)][string]$BuildDir)

$ErrorActionPreference = 'Stop'

$dll = Join-Path $BuildDir 'Release/ArongStudyImeTsf.dll'
$pdb = Join-Path $BuildDir 'Release/ArongStudyImeTsf.pdb'
if (-not (Test-Path $dll)) { throw "The build did not produce $dll" }
if (-not (Test-Path $pdb)) { throw "The build did not produce $pdb" }
$versionInfo = (Get-Item -LiteralPath $dll).VersionInfo
if ($versionInfo.ProductName -ne '阿龍学习输入法') {
    throw "The DLL product name is missing or incorrectly decoded: $($versionInfo.ProductName)"
}
if ($versionInfo.OriginalFilename -ne 'ArongStudyImeTsf.dll') {
    throw "The DLL original filename is incorrect: $($versionInfo.OriginalFilename)"
}
Write-Host "$dll is $((Get-Item $dll).Length) bytes"
Write-Host "$pdb is $((Get-Item $pdb).Length) bytes"
