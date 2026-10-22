"""Compile and run the production path guard without installing the product."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
compiler = shutil.which("ISCC.exe")
if compiler is None:
    compiler = str(Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Inno Setup 6" / "ISCC.exe")
source = (ROOT / "installer" / "msime_setup.iss").read_text(encoding="utf-8-sig")
# Both silent and interactive startup must honor /DATADIR through the production resolver.
assert "DataDirPage.Values[0] := GetDataDir('');" in source
assert "DataDirPage.Values[0] := ResolvePreviousDataDir;" not in source
output = ROOT / "installer" / "Output" / "install-paths-probe"
output.mkdir(parents=True, exist_ok=True)
# Compile the exact production resolver and guard, not Python reimplementations.
start = source.index("function GetDataDir(Param: String): String;")
end = source.index("function DataDirMarkerPath", start)
probe = Path(__file__).with_name("install-paths-probe.iss").read_text(encoding="utf-8-sig")
probe = probe.replace("{ PRODUCTION_DATA_DIR_RESOLVER }", source[start:end])
probe = probe.replace("{ PRODUCTION_PATH_GUARD }", (ROOT / "installer" / "install_paths.iss").read_text(encoding="utf-8-sig"))
probe_file = output / "probe.iss"
probe_file.write_text(probe, encoding="utf-8-sig")
subprocess.run([compiler, "/Qp", "/O" + str(output), str(probe_file)], check=True)
for arguments in ([], ["/DATADIR=D:\\啊隆输入法数据", "/EXPECTED=D:\\啊隆输入法数据"]):
    (output / "passed.txt").write_text("pending")
    result = subprocess.run([str(output / "install-paths-probe.exe"), "/VERYSILENT", "/SUPPRESSMSGBOXES", *arguments], timeout=30)
    # InitializeSetup intentionally returns False; require the marker after all assertions.
    assert result.returncode in (0, 1), result.returncode
    assert (output / "passed.txt").read_text() == "passed"
print("Production path guard: six scenarios; data resolver: previous and explicit paths passed.")
