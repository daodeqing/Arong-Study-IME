[Setup]
AppName=ArongInstallPathsProbe
AppVersion=1
DefaultDirName={src}
PrivilegesRequired=lowest
CreateUninstallRegKey=no
Uninstallable=no
OutputBaseFilename=install-paths-probe
Compression=none

[Code]
{ PRODUCTION_PATH_GUARD }

var
  DataDirValue: String;

function ResolvePreviousDataDir: String;
begin
  Result := 'D:\previous-data';
end;

{ PRODUCTION_DATA_DIR_RESOLVER }

procedure Check(AppDir, DataDir: String; Expected: Boolean);
begin
  if InstallationPathsOverlap(AppDir, DataDir) <> Expected then
    RaiseException('Incorrect path overlap result: ' + AppDir + ' / ' + DataDir);
end;

function InitializeSetup: Boolean;
begin
  Check('D:\啊隆输入法', 'D:\啊隆输入法数据', False);
  Check('D:\啊隆输入法', 'd:\啊隆输入法\', True);
  Check('D:\啊隆输入法', 'D:\啊隆输入法\data', True);
  Check('D:\啊隆输入法\server', 'D:\啊隆输入法', True);
  Check('D:\IME', 'D:\IME-other', False);
  Check('D:\IME\', 'D:\IME\data\', True);
  if GetDataDir('') <> ExpandConstant('{param:EXPECTED|D:\previous-data}') then
    RaiseException('Explicit data directory was ignored');
  SaveStringToFile(ExpandConstant('{src}\passed.txt'), 'passed', False);
  Result := False;
end;
