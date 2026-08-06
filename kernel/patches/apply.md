Apply These Patches
bash

# Navigate to kernel source directory
cd /path/to/linux-kernel

# Apply DDR optimization patch
patch -p1 < /path/to/0001-ddr-optimization.patch

# Apply NPU memory fix patch
patch -p1 < /path/to/0002-npu-memory-fix.patch

# Verify patches applied
git status
git diff

# Build kernel with patches
make -j$(nproc)
make modules_install
make install

# Reboot to test
reboot

# Check if patches are applied
dmesg | grep -E "DDR Optimization|NPU Memory"
