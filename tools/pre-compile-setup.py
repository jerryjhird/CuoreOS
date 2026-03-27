import subprocess
import sys
import os
from pathlib import Path

def run(cmd):
    subprocess.run(cmd, shell=True, check=True)

build_dir = Path(sys.argv[1])
flanterm_dir = build_dir / "flanterm"

if not os.path.exists(flanterm_dir):
    print("Cloning Flanterm")
    run(f"git clone --depth=1 https://codeberg.org/Mintsuki/Flanterm.git {flanterm_dir}")