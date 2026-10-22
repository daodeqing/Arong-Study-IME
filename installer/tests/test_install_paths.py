"""Compile and run the production path guard without installing the product."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
compiler = shutil.which("ISCC.exe")
if compiler is None:
    compiler = str(Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Inno Setup 6" / "ISCC.exe")
output = ROOT / "installer" / "Output" / "install-paths-probe"
output.mkdir(parents=True, exist_ok=True)
subprocess.run([compiler, "/Qp", "/O" + str(output), str(Path(__file__).with_name("install-paths-probe.iss"))], check=True)
(output / "passed.txt").write_text("pending")
result = subprocess.run([str(output / "install-paths-probe.exe"), "/VERYSILENT", "/SUPPRESSMSGBOXES"], timeout=30)
# InitializeSetup intentionally returns False; require the marker written after every assertion.
assert result.returncode in (0, 1), result.returncode
assert (output / "passed.txt").read_text() == "passed"
print("Production installation path guard: six scenarios passed.")
