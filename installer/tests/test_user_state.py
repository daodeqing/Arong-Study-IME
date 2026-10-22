"""Run production upgrade cleanup and user-state migration in an isolated Inno probe."""
from pathlib import Path
import ctypes
from ctypes import wintypes
import json
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "installer/Output/user-state-probe"
FILES = ("ai_providers.json", "user_dict.dat", "clipboard_history.json")


def body(source, name, kind="function"):
    start = source.index(f"{kind} {name}(")
    return source[start:source.index("\nend;", start) + len("\nend;")]


def main():
    compiler = shutil.which("ISCC.exe")
    if compiler is None:
        compiler = str(Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
                       / "Inno Setup 6/ISCC.exe")
    source = (ROOT / "installer/msime_setup.iss").read_text(encoding="utf-8-sig")
    cloud = (ROOT / "installer/cloud_choice.iss").read_text(encoding="utf-8-sig")
    OUT.mkdir(parents=True, exist_ok=True)
    temp = OUT / "temp"
    temp.mkdir(exist_ok=True)
    environment = {**os.environ, "TEMP": str(temp), "TMP": str(temp)}
    predicates = source[source.index("function IsUserDatabaseFile("):
                        source.index("\n{ 0.9.x", source.index("function IsKnownAppDataItem("))]
    code = r"""
[Setup]
AppName=Arong user-state regression probe
AppVersion=1
DefaultDirName={src}
PrivilegesRequired=lowest
Uninstallable=no
CreateAppDir=no
OutputBaseFilename=user-state-probe
Compression=none
[Code]
const DataDirMarkerName = '.ArongStudyIme-data';
var CurrentData: String; Exclusive: Boolean;
function GetDataDir(Param: String): String;
begin Result := CurrentData; end;
function OwnsDataDirExclusively(const Directory: String): Boolean;
begin Result := Exclusive; end;
function DataDirHasMarker(const Directory: String): Boolean;
begin Result := True; end;
procedure TryDeleteTree(const Directory: String);
begin RaiseException('Unexpected directory cleanup'); end;
procedure DeleteKnownAppDataDir(const Directory, FileName: String);
begin RaiseException('Unexpected known directory cleanup'); end;
procedure CleanHelpcodesExceptCustom(const Directory: String);
begin RaiseException('Unexpected helpcode cleanup'); end;
procedure RemoveDataDir(const Directory: String);
begin
  if FileExists(AddBackslash(Directory) + 'ai_providers.json') or
     FileExists(AddBackslash(Directory) + 'user_dict.dat') or
     FileExists(AddBackslash(Directory) + 'clipboard_history.json') then
    RaiseException('Migration would delete unmoved user state');
end;
"""
    code += predicates + "\n"
    for name in ("SameFileContent", "MigratedFileConflicts", "ConfigMigrationProblem"):
        code += body(cloud, name) + "\n"
    for name in ("DropMigratedCopy", "DropMigratedConfigCopy"):
        code += body(cloud, name, "procedure") + "\n"
    for name in ("ConfigMigrationFailure", "TranslationMigrationFailure", "UserStateMigrationFailure"):
        code += body(cloud, name) + "\n"
    code += body(source, "CleanAppDataExceptUserFiles", "procedure") + "\n"
    code += body(source, "MigrateUserDataDir") + "\n"
    code += r"""
function InitializeSetup: Boolean;
var OldDir, NewDir, Problem, Action, FileName: String;
begin
  Action := ExpandConstant('{param:ACTION|cleanup}');
  CurrentData := ExpandConstant('{src}\fixture');
  OldDir := AddBackslash(CurrentData) + 'old';
  NewDir := AddBackslash(CurrentData) + 'new';
  FileName := ExpandConstant('{param:FILE|ai_providers.json}');
  if Action = 'cleanup' then
  begin
    Exclusive := ExpandConstant('{param:EXCLUSIVE|1}') = '1';
    CleanAppDataExceptUserFiles;
  end
  else if Action = 'conflict' then
  begin
    Problem := ConfigMigrationProblem(OldDir, NewDir);
    if Problem = '' then RaiseException('Different target user state was overwritten');
  end
  else
  begin
    Problem := MigrateUserDataDir(OldDir, NewDir);
    if Action = 'locked' then
    begin
      if Problem = '' then RaiseException('Locked source migration silently succeeded');
      if not FileExists(AddBackslash(OldDir) + FileName) then
        RaiseException('Locked user state disappeared');
    end
    else if Problem <> '' then RaiseException(Problem);
  end;
  SaveStringToFile(ExpandConstant('{src}\passed.txt'), 'passed', False);
  Result := False;
end;
"""
    probe = OUT / "probe.iss"
    probe.write_text(code, encoding="utf-8-sig")
    subprocess.run([compiler, "/Qp", "/O" + str(OUT), str(probe)],
                   check=True, env=environment)
    fixture = OUT / "fixture"
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    results = []
    def run(action, name="ai_providers.json", exclusive=True):
        marker = OUT / "passed.txt"
        marker.write_text("pending", encoding="ascii")
        result = subprocess.run([str(OUT / "user-state-probe.exe"), "/VERYSILENT",
                                 "/SUPPRESSMSGBOXES", "/ACTION=" + action,
                                 "/FILE=" + name, "/EXCLUSIVE=" + str(int(exclusive))],
                                timeout=40, env=environment)
        assert result.returncode in (0, 1), result.returncode
        assert marker.read_text() == "passed", (action, name, result.returncode)
    # No fixture is deleted: each completed case is archived for later recycling.
    def prepare(case):
        if fixture.exists():
            fixture.rename(OUT / ("fixture-" + results[-1]))
        fixture.mkdir()
        return fixture
    for exclusive in (True, False):
        prepare("cleanup")
        for name in FILES:
            (fixture / name).write_bytes(b"retained synthetic user state")
        (fixture / "config.toml.tmp").write_bytes(b"rebuildable")
        run("cleanup", exclusive=exclusive)
        for name in FILES:
            assert (fixture / name).read_bytes() == b"retained synthetic user state"
        assert not (fixture / "config.toml.tmp").exists()
        results.append("cleanup-exclusive" if exclusive else "cleanup-shared")
    for name in FILES:
        for action in ("move", "same", "conflict", "locked"):
            prepare(action)
            old, new = fixture / "old", fixture / "new"
            old.mkdir(); new.mkdir()
            (old / name).write_bytes(b"source synthetic state")
            if action in ("same", "conflict"):
                (new / name).write_bytes(b"source synthetic state" if action == "same"
                                        else b"different target state")
            handle = None
            try:
                if action == "locked":
                    handle = kernel.CreateFileW(str(old / name), 0x80000000, 0, None, 3, 0, None)
                    assert handle not in (None, ctypes.c_void_p(-1).value)
                run("move" if action == "same" else action, name)
            finally:
                if handle is not None:
                    kernel.CloseHandle(handle)
            if action in ("move", "same"):
                assert not (old / name).exists()
                assert (new / name).read_bytes() == b"source synthetic state"
            elif action == "conflict":
                assert (old / name).read_bytes() == b"source synthetic state"
                assert (new / name).read_bytes() == b"different target state"
            else:
                assert (old / name).read_bytes() == b"source synthetic state"
                assert not (new / name).exists()
            results.append(action + "-" + name)
    report = {"production_source_extracted": True, "passed": results, "count": len(results)}
    (OUT / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
