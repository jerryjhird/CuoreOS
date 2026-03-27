import subprocess
import sys
import os
import struct
import time
import shutil
import tomllib

def run(cmd):
    subprocess.run(cmd, shell=True, check=True)

def generate_binary_config(source_text):
    try:
        parsed_data = tomllib.loads(source_text)
        config = parsed_data.get("config", {})
    except Exception as e:
        print(f"Error parsing TOML: {e}")
        return None

    schema = [
        ('magic',    0x00000000666E6F63, 'Q'),
        ('epoch',    int(time.time()), 'Q'),
        ('debug',    0, 'B'),
        ('flanterm_is_debug_interface',    0, 'B'),
        ('uart16550_is_debug_interface',    0, 'B'),
    ]

    ordered_values = []
    fmt_str = '<' 
    for key, default, fmt in schema:
        val = config.get(key, default)
        if isinstance(val, bool):
            val = 1 if val else 0
        ordered_values.append(val)
        fmt_str += fmt

    return struct.pack(fmt_str, *ordered_values)

if len(sys.argv) < 5:
    print("Usage: python build.py <out> <kernel> <conf> <build_dir>")
    sys.exit(1)

out_file, kernel, conf, build_dir = sys.argv[1:]

config_path = "../cuore.toml" 
baked_config_path = os.path.join(build_dir, "config.bin")

with open(config_path, "r") as f:
    config_text = f.read()

baked_config = generate_binary_config(config_text)
with open(baked_config_path, "wb") as f:
    f.write(baked_config)

limine_dir = os.path.join(build_dir, "limine")

if not os.path.exists(limine_dir):
    run(f"git clone --depth=1 --branch v10.x-binary https://codeberg.org/Limine/Limine.git {limine_dir}")

print(f"Generating ISO: {out_file}")
iso_root = os.path.abspath(os.path.join(build_dir, "iso_root"))
    
if os.path.exists(iso_root):
    shutil.rmtree(iso_root)
    
os.makedirs(os.path.join(iso_root, "EFI/BOOT"), exist_ok=True)
os.makedirs(os.path.join(iso_root, "boot"), exist_ok=True)

shutil.copy(os.path.join(limine_dir, "BOOTX64.EFI"), os.path.join(iso_root, "EFI/BOOT/BOOTX64.EFI"))
shutil.copy(os.path.join(limine_dir, "limine-uefi-cd.bin"), os.path.join(iso_root, "boot/limine-uefi-cd.bin"))
shutil.copy(kernel, os.path.join(iso_root, "kernel.elf"))
shutil.copy(baked_config_path, os.path.join(iso_root, "boot/cuore.conf.bin"))
shutil.copy(conf, os.path.join(iso_root, "limine.conf"))

run(f"xorriso -as mkisofs -R -J -pad "
    f"-efi-boot-part --efi-boot-image "
    f"--efi-boot boot/limine-uefi-cd.bin "
    f"-no-emul-boot "
    f"-o {out_file} {iso_root}")