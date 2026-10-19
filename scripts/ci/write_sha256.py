"""Write a lowercase SHA-256 sidecar next to a release artifact."""
from hashlib import sha256
from pathlib import Path
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("artifact", type=Path)
args = parser.parse_args()
artifact = args.artifact.resolve()
digest = sha256(artifact.read_bytes()).hexdigest()
sidecar = artifact.with_name(artifact.name + ".sha256")
sidecar.write_text(f"{digest}  {artifact.name}\n", encoding="ascii", newline="\n")
print(f"{artifact.name}: {digest}")
