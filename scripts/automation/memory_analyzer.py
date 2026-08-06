#!/usr/bin/env python3
"""
memory_analyzer.py - Memory Usage Analyzer for RK3568

This script analyzes DDR memory usage, fragmentation, and performance
on RK3568-based systems.

Version: 1.0.0
Author: Sebastian
Date: 2024-08-06

Usage:
    python3 memory_analyzer.py --live
    python3 memory_analyzer.py --analyze --duration 60
    python3 memory_analyzer.py --report --output report.html
    python3 memory_analyzer.py --profile --pid 1234

Requirements:
    pip install psutil pandas matplotlib numpy
"""

import os
import sys
import time
import json
import argparse
import subprocess
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, asdict
from collections import deque
import threading
import signal

try:
    import psutil
    import pandas as pd
    import numpy as np
    import matplotlib.pyplot as plt
    from matplotlib import dates as mdates
except ImportError:
    print("⚠️  Required packages not installed. Run: pip install psutil pandas matplotlib numpy")
    sys.exit(1)

# ============================================================================
# Configuration
# ============================================================================

SAMPLING_INTERVAL = 1.0  # Seconds
MAX_SAMPLES = 3600       # 1 hour at 1s interval
MEMORY_THRESHOLD_WARNING = 85  # Percent
MEMORY_THRESHOLD_CRITICAL = 95  # Percent

# ============================================================================
# Data Classes
# ============================================================================

@dataclass
class MemorySample:
    """Single memory sample"""
    timestamp: float
    total_memory: int
    used_memory: int
    free_memory: int
    cached_memory: int
    buffers_memory: int
    swap_used: int
    swap_total: int
    memory_percent: float
    load_avg: tuple
    cpu_percent: float
    processes: int

@dataclass
class MemoryStats:
    """Memory statistics"""
    samples: List[MemorySample]
    min_usage: float
    max_usage: float
    avg_usage: float
    std_usage: float
    total_allocations: int
    total_frees: int
    peak_usage: int
    fragmentation: float
    timestamp_start: float
    timestamp_end: float

@dataclass
class ProcessMemory:
    """Process memory information"""
    pid: int
    name: str
    memory_rss: int
    memory_vms: int
    memory_percent: float
    cpu_percent: float
    threads: int
    create_time: float

# ============================================================================
# Memory Analyzer
# ============================================================================

class MemoryAnalyzer:
    """
    Memory analyzer for RK3568
    """
    
    def __init__(self, interval: float = SAMPLING_INTERVAL):
        self.interval = interval
        self.samples = deque(maxlen=MAX_SAMPLES)
        self.running = False
        self.analyzer_thread = None
        self.lock = threading.Lock()
        
    def start(self):
        """Start memory analysis"""
        if self.running:
            return
        
        self.running = True
        self.analyzer_thread = threading.Thread(target=self._analyze_loop)
        self.analyzer_thread.daemon = True
        self.analyzer_thread.start()
        print(f"✅ Memory analyzer started (interval: {self.interval}s)")
    
    def stop(self):
        """Stop memory analysis"""
        self.running = False
        if self.analyzer_thread:
            self.analyzer_thread.join(timeout=2.0)
        print("✅ Memory analyzer stopped")
    
    def _analyze_loop(self):
        """Main analysis loop"""
        while self.running:
            sample = self._collect_sample()
            with self.lock:
                self.samples.append(sample)
            
            # Check thresholds
            if sample.memory_percent > MEMORY_THRESHOLD_CRITICAL:
                print(f"⚠️  CRITICAL: Memory usage {sample.memory_percent:.1f}%")
            elif sample.memory_percent > MEMORY_THRESHOLD_WARNING:
                print(f"⚠️  WARNING: Memory usage {sample.memory_percent:.1f}%")
            
            time.sleep(self.interval)
    
    def _collect_sample(self) -> MemorySample:
        """Collect a single memory sample"""
        mem = psutil.virtual_memory()
        swap = psutil.swap_memory()
        load = os.getloadavg()
        cpu = psutil.cpu_percent(interval=0.1)
        
        return MemorySample(
            timestamp=time.time(),
            total_memory=mem.total,
            used_memory=mem.used,
            free_memory=mem.free,
            cached_memory=mem.cached,
            buffers_memory=mem.buffers,
            swap_used=swap.used,
            swap_total=swap.total,
            memory_percent=mem.percent,
            load_avg=load,
            cpu_percent=cpu,
            processes=len(psutil.pids())
        )
    
    def get_stats(self) -> MemoryStats:
        """Get statistics from collected samples"""
        with self.lock:
            samples = list(self.samples)
        
        if not samples:
            return None
        
        usage = [s.memory_percent for s in samples]
        
        return MemoryStats(
            samples=samples,
            min_usage=min(usage),
            max_usage=max(usage),
            avg_usage=np.mean(usage),
            std_usage=np.std(usage),
            total_allocations=0,
            total_frees=0,
            peak_usage=max(s.used_memory for s in samples),
            fragmentation=0.0,
            timestamp_start=samples[0].timestamp,
            timestamp_end=samples[-1].timestamp
        )
    
    def get_processes(self, sort_by: str = 'memory') -> List[ProcessMemory]:
        """Get process memory information"""
        processes = []
        
        for proc in psutil.process_iter(['pid', 'name', 'memory_info', 
                                         'memory_percent', 'cpu_percent',
                                         'num_threads', 'create_time']):
            try:
                info = proc.info
                mem = info['memory_info']
                processes.append(ProcessMemory(
                    pid=info['pid'],
                    name=info['name'],
                    memory_rss=mem.rss if mem else 0,
                    memory_vms=mem.vms if mem else 0,
                    memory_percent=info['memory_percent'] or 0,
                    cpu_percent=info['cpu_percent'] or 0,
                    threads=info['num_threads'] or 0,
                    create_time=info['create_time'] or 0
                ))
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        
        # Sort by memory usage
        if sort_by == 'memory':
            processes.sort(key=lambda p: p.memory_rss, reverse=True)
        elif sort_by == 'cpu':
            processes.sort(key=lambda p: p.cpu_percent, reverse=True)
        
        return processes
    
    def analyze_leaks(self, threshold: int = 1024 * 1024) -> Dict:
        """Analyze for memory leaks"""
        leaks = {}
        processes = self.get_processes()
        
        for proc in processes:
            if proc.memory_rss > threshold:
                leaks[proc.pid] = {
                    'name': proc.name,
                    'memory': proc.memory_rss,
                    'memory_mb': proc.memory_rss / (1024 * 1024),
                    'memory_percent': proc.memory_percent
                }
        
        return leaks

# ============================================================================
# Report Generator
# ============================================================================

class ReportGenerator:
    """
    HTML report generator
    """
    
    def __init__(self, analyzer: MemoryAnalyzer):
        self.analyzer = analyzer
    
    def generate_html(self, output_path: str):
        """Generate HTML report"""
        stats = self.analyzer.get_stats()
        if not stats:
            print("⚠️  No data to generate report")
            return
        
        processes = self.analyzer.get_processes()
        leaks = self.analyzer.analyze_leaks()
        
        html = f"""
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Memory Analysis Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }}
        .container {{ max-width: 1200px; margin: 0 auto; }}
        .card {{ background: white; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin: 20px 0; padding: 20px; }}
        .header {{ background: #2c3e50; color: white; padding: 20px; border-radius: 8px; }}
        .stats-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }}
        .stat-item {{ background: #f8f9fa; padding: 15px; border-radius: 4px; }}
        .stat-value {{ font-size: 24px; font-weight: bold; color: #2c3e50; }}
        .stat-label {{ font-size: 12px; color: #7f8c8d; text-transform: uppercase; }}
        table {{ width: 100%; border-collapse: collapse; }}
        th, td {{ padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }}
        th {{ background: #34495e; color: white; }}
        tr:hover {{ background: #f5f5f5; }}
        .warning {{ color: #f39c12; }}
        .critical {{ color: #e74c3c; }}
        .success {{ color: #27ae60; }}
        .chart-container {{ background: white; padding: 20px; border-radius: 8px; margin: 20px 0; }}
    </style>
</head>
<body>
    <div class="container">
        <div class="card header">
            <h1>📊 Memory Analysis Report</h1>
            <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
            <p>Duration: {stats.timestamp_end - stats.timestamp_start:.1f} seconds</p>
        </div>
        
        <div class="card">
            <h2>📈 Memory Statistics</h2>
            <div class="stats-grid">
                <div class="stat-item">
                    <div class="stat-value">{stats.avg_usage:.1f}%</div>
                    <div class="stat-label">Average Usage</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value">{stats.min_usage:.1f}%</div>
                    <div class="stat-label">Minimum Usage</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value">{stats.max_usage:.1f}%</div>
                    <div class="stat-label">Maximum Usage</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value">{stats.peak_usage / (1024*1024):.1f} MB</div>
                    <div class="stat-label">Peak Usage</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value">{stats.samples[0].total_memory / (1024*1024):.0f} MB</div>
                    <div class="stat-label">Total Memory</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value">{stats.std_usage:.1f}</div>
                    <div class="stat-label">Standard Deviation</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2>🔝 Top Memory Processes</h2>
            <table>
                <tr>
                    <th>PID</th>
                    <th>Name</th>
                    <th>Memory (MB)</th>
                    <th>Memory %</th>
                    <th>CPU %</th>
                    <th>Threads</th>
                </tr>
        """
        
        for proc in processes[:20]:
            html += f"""
                <tr>
                    <td>{proc.pid}</td>
                    <td>{proc.name}</td>
                    <td>{proc.memory_rss / (1024*1024):.1f}</td>
                    <td>{proc.memory_percent:.1f}</td>
                    <td>{proc.cpu_percent:.1f}</td>
                    <td>{proc.threads}</td>
                </tr>
            """
        
        html += """
            </table>
        </div>
        """
        
        if leaks:
            html += """
        <div class="card">
            <h2>⚠️ Potential Memory Leaks</h2>
            <table>
                <tr>
                    <th>PID</th>
                    <th>Name</th>
                    <th>Memory (MB)</th>
                    <th>Memory %</th>
                </tr>
            """
            for pid, info in leaks.items():
                html += f"""
                <tr>
                    <td>{pid}</td>
                    <td>{info['name']}</td>
                    <td class="critical">{info['memory_mb']:.1f}</td>
                    <td class="critical">{info['memory_percent']:.1f}</td>
                </tr>
                """
            html += "</table></div>"
        
        # System info
        html += f"""
        <div class="card">
            <h2>🖥️ System Information</h2>
            <div class="stats-grid">
                <div class="stat-item">
                    <div class="stat-value">{stats.samples[0].processes}</div>
                    <div class="stat-label">Processes</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value">{stats.samples[0].cpu_percent:.1f}%</div>
                    <div class="stat-label">CPU Usage</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value">{stats.samples[0].load_avg[0]:.2f}</div>
                    <div class="stat-label">Load Average (1m)</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value">{stats.samples[0].swap_used / (1024*1024):.0f} MB</div>
                    <div class="stat-label">Swap Used</div>
                </div>
            </div>
        </div>
        """
        
        html += """
    </div>
</body>
</html>
"""
        
        with open(output_path, 'w') as f:
            f.write(html)
        
        print(f"✅ Report generated: {output_path}")
        return html

# ============================================================================
# Command Line Interface
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Memory Usage Analyzer for RK3568'
    )
    
    parser.add_argument(
        '--live',
        action='store_true',
        help='Live monitoring mode'
    )
    
    parser.add_argument(
        '--analyze',
        action='store_true',
        help='Analyze memory usage'
    )
    
    parser.add_argument(
        '--duration',
        type=int,
        default=60,
        help='Analysis duration in seconds (default: 60)'
    )
    
    parser.add_argument(
        '--report',
        action='store_true',
        help='Generate HTML report'
    )
    
    parser.add_argument(
        '--output',
        default='memory_report.html',
        help='Output file for report (default: memory_report.html)'
    )
    
    parser.add_argument(
        '--profile',
        action='store_true',
        help='Profile memory usage'
    )
    
    parser.add_argument(
        '--pid',
        type=int,
        help='Process ID to profile'
    )
    
    parser.add_argument(
        '--interval',
        type=float,
        default=1.0,
        help='Sampling interval in seconds (default: 1.0)'
    )
    
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Verbose output'
    )
    
    args = parser.parse_args()
    
    # Create analyzer
    analyzer = MemoryAnalyzer(args.interval)
    
    # Handle interrupt
    def signal_handler(sig, frame):
        analyzer.stop()
        sys.exit(0)
    signal.signal(signal.SIGINT, signal_handler)
    
    try:
        if args.live:
            # Live monitoring
            analyzer.start()
            print("\n📊 Live Memory Monitoring (Press Ctrl+C to stop)\n")
            
            while True:
                stats = analyzer.get_stats()
                if stats:
                    last = stats.samples[-1]
                    print(f"\rMemory: {last.memory_percent:.1f}% | "
                          f"Used: {last.used_memory/(1024*1024):.0f}MB | "
                          f"Free: {last.free_memory/(1024*1024):.0f}MB | "
                          f"CPU: {last.cpu_percent:.1f}% | "
                          f"Processes: {last.processes}", end="")
                time.sleep(0.5)
        
        elif args.analyze:
            # Analysis mode
            print(f"📊 Analyzing memory for {args.duration} seconds...")
            analyzer.start()
            time.sleep(args.duration)
            analyzer.stop()
            
            stats = analyzer.get_stats()
            if stats:
                print("\n📈 Analysis Results:")
                print(f"  Total Memory: {stats.samples[0].total_memory/(1024*1024):.0f} MB")
                print(f"  Average Usage: {stats.avg_usage:.1f}%")
                print(f"  Peak Usage: {stats.peak_usage/(1024*1024):.1f} MB")
                print(f"  Min Usage: {stats.min_usage:.1f}%")
                print(f"  Max Usage: {stats.max_usage:.1f}%")
                
                processes = analyzer.get_processes()
                print("\n  Top 5 Memory Processes:")
                for proc in processes[:5]:
                    print(f"    {proc.name} (PID {proc.pid}): {proc.memory_rss/(1024*1024):.1f} MB")
        
        elif args.report:
            # Generate report
            print("📊 Generating memory report...")
            analyzer.start()
            time.sleep(10)  # Collect some data
            analyzer.stop()
            
            generator = ReportGenerator(analyzer)
            generator.generate_html(args.output)
        
        elif args.profile and args.pid:
            # Profile specific process
            print(f"📊 Profiling process {args.pid}...")
            try:
                proc = psutil.Process(args.pid)
                print(f"\n  Process: {proc.name()} (PID {proc.pid})")
                print(f"  Memory RSS: {proc.memory_info().rss/(1024*1024):.1f} MB")
                print(f"  Memory VMS: {proc.memory_info().vms/(1024*1024):.1f} MB")
                print(f"  CPU: {proc.cpu_percent():.1f}%")
                print(f"  Threads: {proc.num_threads()}")
                print(f"  Created: {datetime.fromtimestamp(proc.create_time())}")
                
                # Memory map
                print("\n  Memory Maps:")
                for region in proc.memory_maps()[:10]:
                    print(f"    {region.path[:50]}: {region.rss/(1024*1024):.1f} MB")
                
            except psutil.NoSuchProcess:
                print(f"❌ Process {args.pid} not found")
        
        else:
            parser.print_help()
    
    except KeyboardInterrupt:
        analyzer.stop()
        print("\n✅ Analysis stopped")
    
    except Exception as e:
        print(f"❌ Error: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()

if __name__ == "__main__":
    main()
