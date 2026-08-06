#!/usr/bin/env python3
"""
auto_config.py - Automatic DDR Configuration Generator for RK3568

This script automatically generates DDR configuration based on
board type, memory type, and performance requirements.

Version: 1.0.0
Author: Sebastian
Date: 2024-08-06

Usage:
    python3 auto_config.py --board edge2 --profile balanced
    python3 auto_config.py --board rock3b --profile performance
    python3 auto_config.py --board orange5 --profile powersave
    python3 auto_config.py --custom --memory-size 4096 --memory-type lpddr4

Requirements:
    pip install pyyaml json5 click
"""

import os
import sys
import json
import yaml
import argparse
import re
import subprocess
from pathlib import Path
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, asdict
from enum import Enum
import logging
from datetime import datetime

# ============================================================================
# Configuration
# ============================================================================

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# ============================================================================
# Data Classes
# ============================================================================

@dataclass
class DDRTimings:
    """DDR timing parameters"""
    tCL: int = 18
    tRCD: int = 18
    tRP: int = 18
    tRAS: int = 42
    tRFC: int = 350
    tRRD: int = 4
    tWTR: int = 4
    tFAW: int = 16

@dataclass
class DDRConfig:
    """DDR configuration"""
    name: str = "custom"
    memory_type: str = "LPDDR4"
    memory_size: int = 4096
    frequency: int = 1800
    voltage: int = 1100
    timings: DDRTimings = DDRTimings()
    channels: int = 2
    ecc_enabled: bool = False
    power_save: bool = False
    performance_mode: bool = True

@dataclass
class BoardConfig:
    """Board configuration"""
    name: str
    vendor: str
    model: str
    memory_type: str
    memory_size: int
    default_frequency: int
    default_voltage: int
    timings: DDRTimings
    npu_enabled: bool = True
    gpu_enabled: bool = True
    vpu_enabled: bool = True

# ============================================================================
# Board Definitions
# ============================================================================

BOARDS = {
    "edge2": BoardConfig(
        name="Mixtile Edge 2",
        vendor="Mixtile",
        model="Edge 2",
        memory_type="LPDDR4",
        memory_size=4096,
        default_frequency=1800,
        default_voltage=1100,
        timings=DDRTimings(tCL=18, tRCD=18, tRP=18, tRAS=42)
    ),
    "rock3b": BoardConfig(
        name="Radxa ROCK 3B",
        vendor="Radxa",
        model="ROCK 3B",
        memory_type="DDR4",
        memory_size=8192,
        default_frequency=1600,
        default_voltage=1200,
        timings=DDRTimings(tCL=16, tRCD=16, tRP=16, tRAS=36)
    ),
    "orange5": BoardConfig(
        name="Orange Pi 5",
        vendor="Orange Pi",
        model="5",
        memory_type="LPDDR4X",
        memory_size=4096,
        default_frequency=2133,
        default_voltage=1050,
        timings=DDRTimings(tCL=20, tRCD=20, tRP=20, tRAS=48)
    ),
    "custom": BoardConfig(
        name="Custom Board",
        vendor="Custom",
        model="Custom-001",
        memory_type="LPDDR4",
        memory_size=4096,
        default_frequency=1800,
        default_voltage=1100,
        timings=DDRTimings(tCL=18, tRCD=18, tRP=18, tRAS=42)
    )
}

# ============================================================================
# Profile Definitions
# ============================================================================

PROFILES = {
    "balanced": {
        "description": "Balanced performance and power",
        "frequency_multiplier": 1.0,
        "voltage_multiplier": 1.0,
        "power_save": False,
        "performance_mode": True,
        "timings_aggressive": False,
    },
    "performance": {
        "description": "Maximum performance",
        "frequency_multiplier": 1.1,
        "voltage_multiplier": 1.05,
        "power_save": False,
        "performance_mode": True,
        "timings_aggressive": True,
    },
    "powersave": {
        "description": "Maximum power saving",
        "frequency_multiplier": 0.7,
        "voltage_multiplier": 0.9,
        "power_save": True,
        "performance_mode": False,
        "timings_aggressive": False,
    },
    "custom": {
        "description": "Custom configuration",
        "frequency_multiplier": 1.0,
        "voltage_multiplier": 1.0,
        "power_save": False,
        "performance_mode": True,
        "timings_aggressive": False,
    }
}

# ============================================================================
# Configuration Generator
# ============================================================================

class DDRConfigGenerator:
    """
    DDR configuration generator
    """
    
    def __init__(self):
        self.config = None
        self.board = None
        self.profile = None
        
    def generate(self, board_name: str, profile_name: str = "balanced",
                 custom_params: Optional[Dict] = None) -> DDRConfig:
        """
        Generate DDR configuration
        """
        logger.info(f"Generating configuration for {board_name} with {profile_name} profile")
        
        # Get board configuration
        self.board = BOARDS.get(board_name)
        if not self.board:
            raise ValueError(f"Unknown board: {board_name}")
        
        # Get profile
        self.profile = PROFILES.get(profile_name)
        if not self.profile:
            raise ValueError(f"Unknown profile: {profile_name}")
        
        # Start with board defaults
        self.config = DDRConfig(
            name=board_name,
            memory_type=self.board.memory_type,
            memory_size=self.board.memory_size,
            frequency=int(self.board.default_frequency * self.profile["frequency_multiplier"]),
            voltage=int(self.board.default_voltage * self.profile["voltage_multiplier"]),
            timings=DDRTimings(
                tCL=self.board.timings.tCL,
                tRCD=self.board.timings.tRCD,
                tRP=self.board.timings.tRP,
                tRAS=self.board.timings.tRAS,
                tRFC=self.board.timings.tRFC,
                tRRD=self.board.timings.tRRD,
                tWTR=self.board.timings.tWTR,
                tFAW=self.board.timings.tFAW
            ),
            power_save=self.profile["power_save"],
            performance_mode=self.profile["performance_mode"]
        )
        
        # Apply aggressive timings if requested
        if self.profile["timings_aggressive"]:
            self._apply_aggressive_timings()
        
        # Apply custom parameters
        if custom_params:
            self._apply_custom_params(custom_params)
        
        # Validate configuration
        self._validate_config()
        
        logger.info("Configuration generated successfully")
        return self.config
    
    def _apply_aggressive_timings(self):
        """Apply aggressive timing configuration"""
        self.config.timings.tCL -= 2
        self.config.timings.tRCD -= 2
        self.config.timings.tRP -= 2
        self.config.timings.tRAS -= 4
        self.config.timings.tRFC -= 50
        
        # Ensure minimum values
        self.config.timings.tCL = max(14, self.config.timings.tCL)
        self.config.timings.tRCD = max(14, self.config.timings.tRCD)
        self.config.timings.tRP = max(14, self.config.timings.tRP)
        self.config.timings.tRAS = max(32, self.config.timings.tRAS)
        self.config.timings.tRFC = max(300, self.config.timings.tRFC)
    
    def _apply_custom_params(self, params: Dict):
        """Apply custom parameters"""
        if 'frequency' in params:
            self.config.frequency = params['frequency']
        if 'voltage' in params:
            self.config.voltage = params['voltage']
        if 'tCL' in params:
            self.config.timings.tCL = params['tCL']
        if 'tRCD' in params:
            self.config.timings.tRCD = params['tRCD']
        if 'tRP' in params:
            self.config.timings.tRP = params['tRP']
        if 'tRAS' in params:
            self.config.timings.tRAS = params['tRAS']
        if 'memory_size' in params:
            self.config.memory_size = params['memory_size']
        if 'ecc_enabled' in params:
            self.config.ecc_enabled = params['ecc_enabled']
    
    def _validate_config(self):
        """Validate configuration"""
        if self.config.frequency < 800 or self.config.frequency > 2133:
            logger.warning(f"Frequency {self.config.frequency} MHz may be outside safe range")
        
        if self.config.voltage < 900 or self.config.voltage > 1300:
            logger.warning(f"Voltage {self.config.voltage} mV may be outside safe range")
        
        if self.config.memory_size < 1024 or self.config.memory_size > 8192:
            logger.warning(f"Memory size {self.config.memory_size} MB may be unusual")
    
    def export_defconfig(self, output_path: str):
        """Export configuration as defconfig"""
        if not self.config:
            raise ValueError("No configuration generated")
        
        content = f"""# DDR Configuration for {self.board.name}
# Generated by auto_config.py
# Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

#
# Board Information
#
CONFIG_BOARD_NAME="{self.board.name}"
CONFIG_BOARD_VENDOR="{self.board.vendor}"
CONFIG_BOARD_MODEL="{self.board.model}"

#
# Memory Configuration
#
CONFIG_DDR_TYPE="{self.config.memory_type}"
CONFIG_DDR_SIZE={self.config.memory_size}
CONFIG_DDR_CHANNELS={self.config.channels}
CONFIG_DDR_FREQ={self.config.frequency}
CONFIG_DDR_VOLTAGE={self.config.voltage}
CONFIG_DDR_TCL={self.config.timings.tCL}
CONFIG_DDR_TRCD={self.config.timings.tRCD}
CONFIG_DDR_TRP={self.config.timings.tRP}
CONFIG_DDR_TRAS={self.config.timings.tRAS}
CONFIG_DDR_TRFC={self.config.timings.tRFC}
CONFIG_DDR_TRRD={self.config.timings.tRRD}
CONFIG_DDR_TWTR={self.config.timings.tWTR}
CONFIG_DDR_TFAW={self.config.timings.tFAW}
CONFIG_ECC_ENABLED={"y" if self.config.ecc_enabled else "n"}
CONFIG_POWER_SAVE={"y" if self.config.power_save else "n"}
CONFIG_PERFORMANCE_MODE={"y" if self.config.performance_mode else "n"}

#
# Feature Flags
#
CONFIG_NPU_ENABLED={"y" if self.board.npu_enabled else "n"}
CONFIG_GPU_ENABLED={"y" if self.board.gpu_enabled else "n"}
CONFIG_VPU_ENABLED={"y" if self.board.vpu_enabled else "n"}
"""
        
        with open(output_path, 'w') as f:
            f.write(content)
        
        logger.info(f"Defconfig exported to {output_path}")
    
    def export_json(self, output_path: str):
        """Export configuration as JSON"""
        if not self.config:
            raise ValueError("No configuration generated")
        
        data = {
            'board': {
                'name': self.board.name,
                'vendor': self.board.vendor,
                'model': self.board.model,
            },
            'memory': {
                'type': self.config.memory_type,
                'size': self.config.memory_size,
                'frequency': self.config.frequency,
                'voltage': self.config.voltage,
                'channels': self.config.channels,
            },
            'timings': {
                'tCL': self.config.timings.tCL,
                'tRCD': self.config.timings.tRCD,
                'tRP': self.config.timings.tRP,
                'tRAS': self.config.timings.tRAS,
                'tRFC': self.config.timings.tRFC,
                'tRRD': self.config.timings.tRRD,
                'tWTR': self.config.timings.tWTR,
                'tFAW': self.config.timings.tFAW,
            },
            'features': {
                'ecc_enabled': self.config.ecc_enabled,
                'power_save': self.config.power_save,
                'performance_mode': self.config.performance_mode,
                'npu_enabled': self.board.npu_enabled,
                'gpu_enabled': self.board.gpu_enabled,
                'vpu_enabled': self.board.vpu_enabled,
            }
        }
        
        with open(output_path, 'w') as f:
            json.dump(data, f, indent=2)
        
        logger.info(f"JSON exported to {output_path}")
    
    def export_c_header(self, output_path: str):
        """Export configuration as C header"""
        if not self.config:
            raise ValueError("No configuration generated")
        
        content = f"""/*
 * DDR Configuration for {self.board.name}
 * Generated by auto_config.py
 * Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
 */

#ifndef _DDR_CONFIG_AUTO_H_
#define _DDR_CONFIG_AUTO_H_

#define DDR_CONFIG_VERSION "1.0.0"

/* Board Information */
#define BOARD_NAME "{self.board.name}"
#define BOARD_VENDOR "{self.board.vendor}"
#define BOARD_MODEL "{self.board.model}"

/* Memory Configuration */
#define DDR_MEMORY_TYPE "{self.config.memory_type}"
#define DDR_MEMORY_SIZE {self.config.memory_size}
#define DDR_CHANNELS {self.config.channels}
#define DDR_FREQUENCY {self.config.frequency}
#define DDR_VOLTAGE {self.config.voltage}

/* Timing Parameters */
#define DDR_TCL {self.config.timings.tCL}
#define DDR_TRCD {self.config.timings.tRCD}
#define DDR_TRP {self.config.timings.tRP}
#define DDR_TRAS {self.config.timings.tRAS}
#define DDR_TRFC {self.config.timings.tRFC}
#define DDR_TRRD {self.config.timings.tRRD}
#define DDR_TWTR {self.config.timings.tWTR}
#define DDR_TFAW {self.config.timings.tFAW}

/* Features */
#define DDR_ECC_ENABLED {1 if self.config.ecc_enabled else 0}
#define DDR_POWER_SAVE {1 if self.config.power_save else 0}
#define DDR_PERFORMANCE_MODE {1 if self.config.performance_mode else 0}
#define NPU_ENABLED {1 if self.board.npu_enabled else 0}
#define GPU_ENABLED {1 if self.board.gpu_enabled else 0}
#define VPU_ENABLED {1 if self.board.vpu_enabled else 0}

#endif /* _DDR_CONFIG_AUTO_H_ */
"""
        
        with open(output_path, 'w') as f:
            f.write(content)
        
        logger.info(f"C header exported to {output_path}")

# ============================================================================
# Main Function
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Automatic DDR Configuration Generator for RK3568'
    )
    
    parser.add_argument(
        '--board',
        choices=['edge2', 'rock3b', 'orange5', 'custom'],
        default='custom',
        help='Board type (default: custom)'
    )
    
    parser.add_argument(
        '--profile',
        choices=['balanced', 'performance', 'powersave', 'custom'],
        default='balanced',
        help='Performance profile (default: balanced)'
    )
    
    parser.add_argument(
        '--custom-frequency',
        type=int,
        help='Custom frequency in MHz'
    )
    
    parser.add_argument(
        '--custom-voltage',
        type=int,
        help='Custom voltage in mV'
    )
    
    parser.add_argument(
        '--custom-memory-size',
        type=int,
        help='Custom memory size in MB'
    )
    
    parser.add_argument(
        '--enable-ecc',
        action='store_true',
        help='Enable ECC'
    )
    
    parser.add_argument(
        '--output',
        default='ddr_config',
        help='Output file prefix (default: ddr_config)'
    )
    
    parser.add_argument(
        '--format',
        choices=['defconfig', 'json', 'c', 'all'],
        default='all',
        help='Output format (default: all)'
    )
    
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Verbose output'
    )
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    # Build custom parameters
    custom_params = {}
    if args.custom_frequency:
        custom_params['frequency'] = args.custom_frequency
    if args.custom_voltage:
        custom_params['voltage'] = args.custom_voltage
    if args.custom_memory_size:
        custom_params['memory_size'] = args.custom_memory_size
    if args.enable_ecc:
        custom_params['ecc_enabled'] = True
    
    # Generate configuration
    generator = DDRConfigGenerator()
    config = generator.generate(args.board, args.profile, custom_params)
    
    # Print configuration
    print("\n=== DDR Configuration ===\n")
    print(f"Board: {config.name}")
    print(f"Memory Type: {config.memory_type}")
    print(f"Memory Size: {config.memory_size} MB")
    print(f"Frequency: {config.frequency} MHz")
    print(f"Voltage: {config.voltage} mV")
    print(f"Timings: CL{config.timings.tCL}-tRCD{config.timings.tRCD}-tRP{config.timings.tRP}-tRAS{config.timings.tRAS}")
    print(f"ECC: {'Enabled' if config.ecc_enabled else 'Disabled'}")
    print(f"Power Save: {'Enabled' if config.power_save else 'Disabled'}")
    print(f"Performance Mode: {'Enabled' if config.performance_mode else 'Disabled'}")
    
    # Export configurations
    if args.format in ['defconfig', 'all']:
        generator.export_defconfig(f"{args.output}.defconfig")
    
    if args.format in ['json', 'all']:
        generator.export_json(f"{args.output}.json")
    
    if args.format in ['c', 'all']:
        generator.export_c_header(f"{args.output}.h")
    
    print("\n✅ Configuration generated successfully!")

if __name__ == "__main__":
    main()
