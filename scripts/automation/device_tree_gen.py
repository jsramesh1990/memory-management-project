#!/usr/bin/env python3
"""
device_tree_gen.py - Device Tree Generator for RK3568

This script generates device tree files for RK3568-based boards
with custom DDR memory configurations.

Version: 1.0.0
Author: Sebastian
Date: 2024-08-06

Usage:
    python3 device_tree_gen.py --board edge2 --output rk3568-edge2.dts
    python3 device_tree_gen.py --board rock3b --memory-size 8192
    python3 device_tree_gen.py --custom --memory-start 0x40000000

Requirements:
    pip install pyyaml
"""

import os
import sys
import argparse
import re
from pathlib import Path
from typing import Dict, List, Optional
from dataclasses import dataclass
from datetime import datetime
import logging

# ============================================================================
# Configuration
# ============================================================================

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# ============================================================================
# Data Classes
# ============================================================================

@dataclass
class MemoryRegion:
    """Memory region descriptor"""
    name: str
    start: int
    size: int
    flags: List[str]
    compatible: Optional[str] = None

@dataclass
class DeviceTreeConfig:
    """Device tree configuration"""
    board_name: str
    board_compatible: str
    memory_size: int
    memory_start: int
    regions: List[MemoryRegion]
    npu_enabled: bool = True
    gpu_enabled: bool = True
    vpu_enabled: bool = True
    features: Dict[str, bool] = None
    
    def __post_init__(self):
        if self.features is None:
            self.features = {}

# ============================================================================
# Board Definitions
# ============================================================================

BOARD_REGIONS = {
    "edge2": [
        MemoryRegion("bootloader", 0x00000000, 0x01000000, ["no-map"]),
        MemoryRegion("uboot_env", 0x01000000, 0x01000000, ["no-map"]),
        MemoryRegion("kernel", 0x02000000, 0x0E000000, ["no-map"]),
        MemoryRegion("dtb", 0x10000000, 0x01000000, ["no-map"]),
        MemoryRegion("reserved", 0x11000000, 0x0F000000, ["no-map"]),
        MemoryRegion("npu", 0x20000000, 0x08000000, ["no-map"]),
        MemoryRegion("gpu", 0x28000000, 0x08000000, ["no-map"]),
        MemoryRegion("vpu", 0x30000000, 0x10000000, ["reusable"], 
                     "shared-dma-pool"),
    ],
    "rock3b": [
        MemoryRegion("bootloader", 0x00000000, 0x01000000, ["no-map"]),
        MemoryRegion("uboot_env", 0x01000000, 0x01000000, ["no-map"]),
        MemoryRegion("kernel", 0x02000000, 0x0E000000, ["no-map"]),
        MemoryRegion("dtb", 0x10000000, 0x01000000, ["no-map"]),
        MemoryRegion("reserved", 0x11000000, 0x0F000000, ["no-map"]),
        MemoryRegion("npu", 0x20000000, 0x10000000, ["no-map"]),
        MemoryRegion("gpu", 0x30000000, 0x10000000, ["no-map"]),
        MemoryRegion("vpu", 0x40000000, 0x20000000, ["reusable"],
                     "shared-dma-pool"),
    ],
    "orange5": [
        MemoryRegion("bootloader", 0x00000000, 0x01000000, ["no-map"]),
        MemoryRegion("uboot_env", 0x01000000, 0x01000000, ["no-map"]),
        MemoryRegion("kernel", 0x02000000, 0x0E000000, ["no-map"]),
        MemoryRegion("dtb", 0x10000000, 0x01000000, ["no-map"]),
        MemoryRegion("reserved", 0x11000000, 0x0F000000, ["no-map"]),
        MemoryRegion("npu", 0x20000000, 0x10000000, ["no-map"]),
        MemoryRegion("gpu", 0x30000000, 0x10000000, ["no-map"]),
        MemoryRegion("vpu", 0x40000000, 0x10000000, ["reusable"],
                     "shared-dma-pool"),
    ],
}

# ============================================================================
# Device Tree Generator
# ============================================================================

class DeviceTreeGenerator:
    """
    Device Tree generator for RK3568
    """
    
    def __init__(self):
        self.config = None
        self.indent = 0
    
    def generate(self, board_name: str, memory_size: int = None,
                 custom_regions: List[MemoryRegion] = None) -> str:
        """
        Generate device tree source
        """
        logger.info(f"Generating device tree for {board_name}")
        
        # Get board configuration
        regions = custom_regions if custom_regions else BOARD_REGIONS.get(board_name)
        if not regions:
            raise ValueError(f"Unknown board: {board_name}")
        
        # Create config
        self.config = DeviceTreeConfig(
            board_name=board_name,
            board_compatible=f"{board_name},custom",
            memory_size=memory_size or 4096,
            memory_start=0x40000000,
            regions=regions
        )
        
        # Generate DTS
        dts = self._generate_dts()
        
        logger.info("Device tree generated successfully")
        return dts
    
    def _generate_dts(self) -> str:
        """Generate complete device tree source"""
        lines = []
        
        # Header
        lines.append("// SPDX-License-Identifier: (GPL-2.0+ OR MIT)")
        lines.append("/*")
        lines.append(f" * Device Tree for {self.config.board_name}")
        lines.append(" * Generated by device_tree_gen.py")
        lines.append(f" * Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append(" */")
        lines.append("")
        lines.append("/dts-v1/;")
        lines.append('#include "rk3568.dtsi"')
        lines.append("")
        
        # Root node
        lines.append("/ {")
        self.indent = 1
        
        # Model and compatible
        lines.append(self._indent(f'model = "{self.config.board_name}";'))
        lines.append(self._indent(f'compatible = "{self.config.board_compatible}", "rockchip,rk3568";'))
        lines.append("")
        
        # Memory node
        lines.extend(self._generate_memory_node())
        lines.append("")
        
        # Reserved memory
        lines.extend(self._generate_reserved_memory())
        lines.append("")
        
        # Chosen node
        lines.extend(self._generate_chosen_node())
        lines.append("")
        
        # Features
        if self.config.npu_enabled:
            lines.extend(self._generate_npu_node())
        if self.config.gpu_enabled:
            lines.extend(self._generate_gpu_node())
        if self.config.vpu_enabled:
            lines.extend(self._generate_vpu_node())
        
        # Close root
        self.indent = 0
        lines.append("};")
        
        return "\n".join(lines)
    
    def _indent(self, text: str) -> str:
        """Add indentation to text"""
        return "    " * self.indent + text
    
    def _generate_memory_node(self) -> List[str]:
        """Generate memory node"""
        lines = []
        lines.append(self._indent("memory@0 {"))
        self.indent += 1
        
        lines.append(self._indent('device_type = "memory";'))
        lines.append(self._indent(f'reg = <0x0 0x{self.config.memory_start:08x} 0x0 0x{self.config.memory_size:08x} 0x0 0x0>;'))
        
        self.indent -= 1
        lines.append(self._indent("};"))
        return lines
    
    def _generate_reserved_memory(self) -> List[str]:
        """Generate reserved memory node"""
        lines = []
        lines.append(self._indent("reserved-memory {"))
        self.indent += 1
        
        lines.append(self._indent("#address-cells = <2>;"))
        lines.append(self._indent("#size-cells = <2>;"))
        lines.append(self._indent("ranges;"))
        lines.append("")
        
        for region in self.config.regions:
            lines.extend(self._generate_region(region))
            lines.append("")
        
        self.indent -= 1
        lines.append(self._indent("};"))
        return lines
    
    def _generate_region(self, region: MemoryRegion) -> List[str]:
        """Generate a memory region"""
        lines = []
        lines.append(self._indent(f'{region.name}: {region.name}@{region.start:08x} {{'))
        self.indent += 1
        
        lines.append(self._indent(f'reg = <0x0 0x{region.start:08x} 0x0 0x{region.size:08x}>;'))
        
        if region.compatible:
            lines.append(self._indent(f'compatible = "{region.compatible}";'))
        
        for flag in region.flags:
            lines.append(self._indent(f'{flag};'))
        
        lines.append(self._indent('status = "okay";'))
        
        self.indent -= 1
        lines.append(self._indent("};"))
        return lines
    
    def _generate_chosen_node(self) -> List[str]:
        """Generate chosen node"""
        lines = []
        lines.append(self._indent("chosen {"))
        self.indent += 1
        
        lines.append(self._indent('stdout-path = &uart2;'))
        lines.append(self._indent('bootargs = "console=ttyS2,1500000 earlycon=uart8250,mmio32,0xfe660000 root=/dev/mmcblk0p5 rw rootwait";'))
        
        self.indent -= 1
        lines.append(self._indent("};"))
        return lines
    
    def _generate_npu_node(self) -> List[str]:
        """Generate NPU node"""
        lines = []
        lines.append("")
        lines.append(self._indent("&npu {"))
        self.indent += 1
        
        lines.append(self._indent('status = "okay";'))
        lines.append(self._indent('rockchip,memory-reserved = <&npu>;'))
        
        self.indent -= 1
        lines.append(self._indent("};"))
        return lines
    
    def _generate_gpu_node(self) -> List[str]:
        """Generate GPU node"""
        lines = []
        lines.append("")
        lines.append(self._indent("&gpu {"))
        self.indent += 1
        
        lines.append(self._indent('status = "okay";'))
        lines.append(self._indent('mali-supply = <&vdd_gpu>;'))
        
        self.indent -= 1
        lines.append(self._indent("};"))
        return lines
    
    def _generate_vpu_node(self) -> List[str]:
        """Generate VPU node"""
        lines = []
        lines.append("")
        lines.append(self._indent("&vpu {"))
        self.indent += 1
        
        lines.append(self._indent('status = "okay";'))
        lines.append(self._indent('rockchip,memory-reserved = <&vpu>;'))
        
        self.indent -= 1
        lines.append(self._indent("};"))
        return lines
    
    def save(self, output_path: str, dts_content: str):
        """Save device tree to file"""
        with open(output_path, 'w') as f:
            f.write(dts_content)
        logger.info(f"Device tree saved to {output_path}")
    
    def compile(self, dts_path: str, dtb_path: str = None):
        """Compile device tree to binary"""
        if dtb_path is None:
            dtb_path = dts_path.replace('.dts', '.dtb')
        
        cmd = f"dtc -I dts -O dtb -o {dtb_path} {dts_path}"
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        
        if result.returncode != 0:
            logger.error(f"Failed to compile device tree: {result.stderr}")
            return False
        
        logger.info(f"Device tree compiled to {dtb_path}")
        return True

# ============================================================================
# Main Function
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Device Tree Generator for RK3568'
    )
    
    parser.add_argument(
        '--board',
        choices=['edge2', 'rock3b', 'orange5', 'custom'],
        default='custom',
        help='Board type (default: custom)'
    )
    
    parser.add_argument(
        '--memory-size',
        type=int,
        default=4096,
        help='Memory size in MB (default: 4096)'
    )
    
    parser.add_argument(
        '--output',
        default='rk3568-custom.dts',
        help='Output DTS file (default: rk3568-custom.dts)'
    )
    
    parser.add_argument(
        '--compile',
        action='store_true',
        help='Compile DTS to DTB'
    )
    
    parser.add_argument(
        '--disable-npu',
        action='store_true',
        help='Disable NPU'
    )
    
    parser.add_argument(
        '--disable-gpu',
        action='store_true',
        help='Disable GPU'
    )
    
    parser.add_argument(
        '--disable-vpu',
        action='store_true',
        help='Disable VPU'
    )
    
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Verbose output'
    )
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    # Generate device tree
    generator = DeviceTreeGenerator()
    dts_content = generator.generate(args.board, args.memory_size)
    
    # Apply feature flags
    if args.disable_npu:
        generator.config.npu_enabled = False
    if args.disable_gpu:
        generator.config.gpu_enabled = False
    if args.disable_vpu:
        generator.config.vpu_enabled = False
    
    # Save DTS
    generator.save(args.output, dts_content)
    
    # Compile if requested
    if args.compile:
        generator.compile(args.output)
    
    print("\n✅ Device tree generated successfully!")

if __name__ == "__main__":
    main()
