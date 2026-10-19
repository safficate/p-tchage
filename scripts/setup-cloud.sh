#!/usr/bin/env bash
# Debian cloud workspace setup without root or changes to system packages.
set -euo pipefail
tools=/workspace/.cloud-tools
mkdir -p "$tools"
if [ ! -x "$tools/venv/bin/python" ]; then python3 -m venv "$tools/venv"; fi
"$tools/venv/bin/python" -m pip install --disable-pip-version-check --no-cache-dir cmake==3.31.10 ninja==1.11.1.4
if [ ! -f "$tools/sysroot/usr/include/alsa/asoundlib.h" ] || [ ! -x "$tools/sysroot/usr/bin/Xvfb" ]; then
    mkdir -p "$tools/apt/parts" "$tools/apt/lists/partial" "$tools/apt/archives/partial" "$tools/sysroot"
    cat > "$tools/apt/apt.conf" <<'CONF'
Dir::Etc::Parts "/workspace/.cloud-tools/apt/parts";
Dir::Etc::main "/workspace/.cloud-tools/apt/none";
Dir::Etc::sourcelist "/workspace/.cloud-tools/apt/sources.list";
Dir::Etc::sourceparts "/workspace/.cloud-tools/apt/parts";
Dir::State::lists "/workspace/.cloud-tools/apt/lists";
Dir::Cache::archives "/workspace/.cloud-tools/apt/archives";
APT::Sandbox::User "agent";
CONF
    printf '%s\n' 'deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main' > "$tools/apt/sources.list"
    touch "$tools/apt/empty.status"
    export APT_CONFIG="$tools/apt/apt.conf"
    /usr/bin/apt-get update
    /usr/bin/apt-get --download-only -o "Dir::State::status=$tools/apt/empty.status" -o Debug::NoLocking=true install -y --no-install-recommends libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev libfreetype-dev libfontconfig-dev libgl-dev libxrender-dev xvfb xauth
    python3 - <<'PY'
import glob, subprocess
for package in glob.glob('/workspace/.cloud-tools/apt/archives/*.deb'):
    subprocess.run(['dpkg-deb', '-x', package, '/workspace/.cloud-tools/sysroot'], check=True)
PY
fi
mkdir -p "$tools/runtime-libs" "$tools/font-cache"
python3 - <<'PY'
from pathlib import Path
root = Path('/workspace/.cloud-tools/sysroot/usr/lib/x86_64-linux-gnu')
output = Path('/workspace/.cloud-tools/runtime-libs')
for lib in root.glob('*.so*'):
    if lib.name.startswith(('libc.so', 'libm.so', 'libpthread.so', 'librt.so', 'libdl.so', 'ld-linux', 'libc_malloc', 'libBrokenLocale', 'libthread_db', 'libresolv', 'libnss_')):
        continue
    dest = output / lib.name
    if not dest.exists(): dest.symlink_to(lib)
PY
cat > "$tools/activate-dev.sh" <<'ACTIVATE'
export PATH="/workspace/.cloud-tools/venv/bin:/workspace/.cloud-tools/sysroot/usr/bin:$PATH"
export CPATH="/workspace/.cloud-tools/sysroot/usr/include:/workspace/.cloud-tools/sysroot/usr/include/x86_64-linux-gnu"
export LIBRARY_PATH="/workspace/.cloud-tools/sysroot/usr/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="/workspace/.cloud-tools/runtime-libs"
export PKG_CONFIG_SYSROOT_DIR="/workspace/.cloud-tools/sysroot"
export PKG_CONFIG_LIBDIR="/workspace/.cloud-tools/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:/workspace/.cloud-tools/sysroot/usr/share/pkgconfig"
ACTIVATE
cat > "$tools/fonts.conf" <<'FONTS'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig><dir>/workspace/.cloud-tools/sysroot/usr/share/fonts/truetype/dejavu</dir><cachedir>/workspace/.cloud-tools/font-cache</cachedir></fontconfig>
FONTS
echo 'Cloud tools prepared. Source /workspace/.cloud-tools/activate-dev.sh before CMake.'
