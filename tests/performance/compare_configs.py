
---

### **2. compare_configs.py**

```python
#!/usr/bin/env python3
"""
compare_configs.py - DDR Configuration Comparison Tool

This script compares DDR configurations and generates performance reports.

Version: 1.0.0
Author: Sebastian
Date: 2024-08-06

Usage:
    python3 compare_configs.py --config1 edge2.json --config2 rock3b.json
    python3 compare_configs.py --dir configs/ --output report.html
    python3 compare_configs.py --all --format markdown

Requirements:
    pip install pandas matplotlib numpy jinja2
"""

import os
import sys
import json
import argparse
import glob
from datetime import datetime
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, asdict
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from jinja2 import Template
import hashlib

# ============================================================================
# Configuration
# ============================================================================

try:
    import pandas as pd
except ImportError:
    print("⚠️  pandas not installed. Run: pip install pandas")
    sys.exit(1)

try:
    import matplotlib.pyplot as plt
except ImportError:
    print("⚠️  matplotlib not installed. Run: pip install matplotlib")
    sys.exit(1)

# ============================================================================
# Data Classes
# ============================================================================

@dataclass
class DDRConfig:
    """DDR Configuration"""
    name: str
    board: str
    type: str
    frequency: int
    voltage: int
    tCL: int
    tRCD: int
    tRP: int
    tRAS: int
    tRFC: int
    ecc: bool
    power_save: bool
    performance_mode: bool
    
    @classmethod
    def from_dict(cls, data: Dict) -> 'DDRConfig':
        return cls(
            name=data.get('name', 'Unknown'),
            board=data.get('board', 'Unknown'),
            type=data.get('type', 'Unknown'),
            frequency=data.get('frequency', 0),
            voltage=data.get('voltage', 0),
            tCL=data.get('tCL', 0),
            tRCD=data.get('tRCD', 0),
            tRP=data.get('tRP', 0),
            tRAS=data.get('tRAS', 0),
            tRFC=data.get('tRFC', 0),
            ecc=data.get('ecc', False),
            power_save=data.get('power_save', False),
            performance_mode=data.get('performance_mode', False)
        )

@dataclass
class ComparisonResult:
    """Comparison result"""
    config1: str
    config2: str
    differences: Dict[str, Any]
    performance_impact: Dict[str, float]
    recommendation: str

# ============================================================================
# Configuration Loader
# ============================================================================

class ConfigLoader:
    """Load DDR configurations from various sources"""
    
    @staticmethod
    def load_json(filepath: str) -> DDRConfig:
        """Load configuration from JSON file"""
        with open(filepath, 'r') as f:
            data = json.load(f)
        return DDRConfig.from_dict(data)
    
    @staticmethod
    def load_defconfig(filepath: str) -> DDRConfig:
        """Load configuration from defconfig file"""
        config = {}
        with open(filepath, 'r') as f:
            for line in f:
                if '=' in line and not line.startswith('#'):
                    key, value = line.strip().split('=', 1)
                    key = key.replace('CONFIG_', '').lower()
                    config[key] = value
        
        return DDRConfig(
            name=config.get('board_name', 'Unknown'),
            board=config.get('board_vendor', 'Unknown') + ' ' + config.get('board_model', ''),
            type=config.get('ddr_type', 'Unknown'),
            frequency=int(config.get('ddr_freq', 0)),
            voltage=int(config.get('ddr_voltage', 0)),
            tCL=int(config.get('ddr_tcl', 0)),
            tRCD=int(config.get('ddr_trcd', 0)),
            tRP=int(config.get('ddr_trp', 0)),
            tRAS=int(config.get('ddr_tras', 0)),
            tRFC=int(config.get('ddr_trfc', 0)),
            ecc=config.get('ecc_enabled', 'n') == 'y',
            power_save=config.get('power_save', 'n') == 'y',
            performance_mode=config.get('performance_mode', 'n') == 'y'
        )
    
    @staticmethod
    def load_directory(directory: str, pattern: str = '*') -> List[DDRConfig]:
        """Load all configurations from a directory"""
        configs = []
        for filepath in glob.glob(os.path.join(directory, pattern)):
            try:
                if filepath.endswith('.json'):
                    configs.append(ConfigLoader.load_json(filepath))
                elif filepath.endswith('.defconfig'):
                    configs.append(ConfigLoader.load_defconfig(filepath))
            except Exception as e:
                print(f"⚠️  Failed to load {filepath}: {e}")
        return configs

# ============================================================================
# Comparison Engine
# ============================================================================

class ComparisonEngine:
    """Compare DDR configurations"""
    
    @staticmethod
    def compare(config1: DDRConfig, config2: DDRConfig) -> ComparisonResult:
        """Compare two configurations"""
        differences = {}
        performance_impact = {}
        
        # Compare parameters
        params = ['frequency', 'voltage', 'tCL', 'tRCD', 'tRP', 'tRAS', 'tRFC']
        for param in params:
            v1 = getattr(config1, param)
            v2 = getattr(config2, param)
            if v1 != v2:
                diff = v2 - v1
                pct = (diff / v1 * 100) if v1 != 0 else 0
                differences[param] = {'value1': v1, 'value2': v2, 'diff': diff, 'pct': pct}
                
                # Calculate performance impact
                if param in ['frequency']:
                    performance_impact['bandwidth'] = pct
                elif param in ['tCL', 'tRCD', 'tRP', 'tRAS']:
                    performance_impact['latency'] = -pct / 2
        
        # Compare features
        features = ['ecc', 'power_save', 'performance_mode']
        for feature in features:
            v1 = getattr(config1, feature)
            v2 = getattr(config2, feature)
            if v1 != v2:
                differences[feature] = {'value1': v1, 'value2': v2}
        
        # Generate recommendation
        recommendation = ComparisonEngine._generate_recommendation(config1, config2, differences)
        
        return ComparisonResult(
            config1=config1.name,
            config2=config2.name,
            differences=differences,
            performance_impact=performance_impact,
            recommendation=recommendation
        )
    
    @staticmethod
    def _generate_recommendation(config1: DDRConfig, config2: DDRConfig, differences: Dict) -> str:
        """Generate recommendation based on comparison"""
        recommendations = []
        
        # Frequency
        if 'frequency' in differences:
            if differences['frequency']['value2'] > differences['frequency']['value1']:
                recommendations.append(f"Increase frequency from {differences['frequency']['value1']} to {differences['frequency']['value2']} MHz for better performance")
            else:
                recommendations.append(f"Lower frequency to {differences['frequency']['value2']} MHz for better power efficiency")
        
        # Timings
        if 'tCL' in differences:
            if differences['tCL']['value2'] < differences['tCL']['value1']:
                recommendations.append(f"Lower CAS latency to {differences['tCL']['value2']} for better performance")
            else:
                recommendations.append(f"Higher CAS latency ({differences['tCL']['value2']}) may improve stability")
        
        # Features
        if 'ecc' in differences:
            if differences['ecc']['value2']:
                recommendations.append("Enable ECC for better reliability")
            else:
                recommendations.append("Disable ECC for better performance")
        
        if 'power_save' in differences:
            if differences['power_save']['value2']:
                recommendations.append("Enable power save mode for lower consumption")
            else:
                recommendations.append("Disable power save mode for better performance")
        
        if not recommendations:
            recommendations.append("Configurations are identical")
        
        return " ".join(recommendations)

# ============================================================================
# Report Generator
# ============================================================================

class ReportGenerator:
    """Generate comparison reports"""
    
    def __init__(self, output_dir: str = './reports'):
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)
    
    def generate_html(self, results: List[ComparisonResult], configs: List[DDRConfig]) -> str:
        """Generate HTML report"""
        template = Template("""
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>DDR Configuration Comparison Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; }
        .header { background: #2c3e50; color: white; padding: 20px; border-radius: 8px; margin-bottom: 20px; }
        .card { background: white; padding: 20px; border-radius: 8px; margin: 20px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        table { width: 100%; border-collapse: collapse; }
        th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background: #34495e; color: white; }
        tr:hover { background: #f5f5f5; }
        .diff-positive { color: #27ae60; }
        .diff-negative { color: #e74c3c; }
        .badge { display: inline-block; padding: 3px 8px; border-radius: 4px; color: white; font-size: 12px; }
        .badge-success { background: #27ae60; }
        .badge-warning { background: #f39c12; }
        .badge-danger { background: #e74c3c; }
        .chart-container { margin: 20px 0; text-align: center; }
        .recommendation { background: #e8f8f5; padding: 15px; border-left: 4px solid #1abc9c; margin: 10px 0; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📊 DDR Configuration Comparison Report</h1>
            <p>Generated: {{ timestamp }}</p>
            <p>Configurations: {{ configs|length }}</p>
        </div>
        
        <div class="card">
            <h2>📋 Configuration Summary</h2>
            <table>
                <tr>
                    <th>Name</th>
                    <th>Board</th>
                    <th>Type</th>
                    <th>Freq (MHz)</th>
                    <th>Voltage (mV)</th>
                    <th>Timings</th>
                    <th>ECC</th>
                    <th>Power Save</th>
                    <th>Performance</th>
                </tr>
                {% for config in configs %}
                <tr>
                    <td><strong>{{ config.name }}</strong></td>
                    <td>{{ config.board }}</td>
                    <td>{{ config.type }}</td>
                    <td>{{ config.frequency }}</td>
                    <td>{{ config.voltage }}</td>
                    <td>CL{{ config.tCL }}-{{ config.tRCD }}-{{ config.tRP }}-{{ config.tRAS }}</td>
                    <td>{% if config.ecc %}<span class="badge badge-success">✓</span>{% else %}<span class="badge badge-warning">✗</span>{% endif %}</td>
                    <td>{% if config.power_save %}<span class="badge badge-success">✓</span>{% else %}<span class="badge badge-warning">✗</span>{% endif %}</td>
                    <td>{% if config.performance_mode %}<span class="badge badge-success">✓</span>{% else %}<span class="badge badge-warning">✗</span>{% endif %}</td>
                </tr>
                {% endfor %}
            </table>
        </div>
        
        <div class="card">
            <h2>🔍 Comparison Results</h2>
            {% for result in results %}
            <h3>{{ result.config1 }} vs {{ result.config2 }}</h3>
            <table>
                <tr>
                    <th>Parameter</th>
                    <th>{{ result.config1 }}</th>
                    <th>{{ result.config2 }}</th>
                    <th>Difference</th>
                    <th>Impact</th>
                </tr>
                {% for key, diff in result.differences.items() %}
                <tr>
                    <td><strong>{{ key }}</strong></td>
                    <td>{{ diff.value1 }}</td>
                    <td>{{ diff.value2 }}</td>
                    <td class="{% if diff.diff > 0 %}diff-positive{% else %}diff-negative{% endif %}">
                        {{ diff.diff }} ({{ "%.1f"|format(diff.pct) }}%)
                    </td>
                    <td>
                        {% if key in result.performance_impact %}
                            {{ "%.1f"|format(result.performance_impact[key]) }}%
                        {% else %}
                            -
                        {% endif %}
                    </td>
                </tr>
                {% endfor %}
            </table>
            <div class="recommendation">
                <strong>💡 Recommendation:</strong> {{ result.recommendation }}
            </div>
            {% endfor %}
        </div>
        
        <div class="card">
            <h2>📈 Performance Impact</h2>
            <div class="chart-container">
                <img src="performance_chart.png" alt="Performance Impact" style="max-width: 100%;">
            </div>
        </div>
    </div>
</body>
</html>
        """)
        
        # Generate performance chart
        self._generate_performance_chart(results)
        
        return template.render(
            timestamp=datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
            configs=configs,
            results=results
        )
    
    def _generate_performance_chart(self, results: List[ComparisonResult]):
        """Generate performance impact chart"""
        fig, ax = plt.subplots(figsize=(10, 6))
        
        for result in results:
            impacts = result.performance_impact
            if impacts:
                labels = list(impacts.keys())
                values = list(impacts.values())
                ax.bar(labels, values, label=f"{result.config1} vs {result.config2}")
        
        ax.set_xlabel('Metric')
        ax.set_ylabel('Impact (%)')
        ax.set_title('Performance Impact Comparison')
        ax.axhline(y=0, color='gray', linestyle='--')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(os.path.join(self.output_dir, 'performance_chart.png'), dpi=150)
        plt.close()
    
    def generate_markdown(self, results: List[ComparisonResult]) -> str:
        """Generate Markdown report"""
        md = f"# DDR Configuration Comparison Report\n\n"
        md += f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n"
        
        for result in results:
            md += f"## {result.config1} vs {result.config2}\n\n"
            md += "| Parameter | Value 1 | Value 2 | Difference |\n"
            md += "|-----------|---------|---------|------------|\n"
            
            for key, diff in result.differences.items():
                md += f"| {key} | {diff['value1']} | {diff['value2']} | {diff['diff']} ({diff['pct']:.1f}%) |\n"
            
            md += f"\n**Recommendation:** {result.recommendation}\n\n"
        
        return md
    
    def generate_csv(self, results: List[ComparisonResult]) -> str:
        """Generate CSV report"""
        rows = []
        for result in results:
            row = {
                'config1': result.config1,
                'config2': result.config2,
                'recommendation': result.recommendation
            }
            for key, diff in result.differences.items():
                row[f'{key}_value1'] = diff['value1']
                row[f'{key}_value2'] = diff['value2']
                row[f'{key}_diff'] = diff['diff']
                row[f'{key}_pct'] = diff['pct']
            rows.append(row)
        
        df = pd.DataFrame(rows)
        return df.to_csv(index=False)

# ============================================================================
# Main Function
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='DDR Configuration Comparison Tool'
    )
    
    parser.add_argument(
        '--config1',
        help='First configuration file'
    )
    
    parser.add_argument(
        '--config2',
        help='Second configuration file'
    )
    
    parser.add_argument(
        '--dir',
        help='Directory containing configuration files'
    )
    
    parser.add_argument(
        '--all',
        action='store_true',
        help='Compare all configurations in directory'
    )
    
    parser.add_argument(
        '--format',
        choices=['html', 'markdown', 'csv', 'all'],
        default='html',
        help='Output format (default: html)'
    )
    
    parser.add_argument(
        '--output',
        default='comparison_report',
        help='Output filename (without extension)'
    )
    
    parser.add_argument(
        '--pattern',
        default='*.json',
        help='File pattern for directory search'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Verbose output'
    )
    
    args = parser.parse_args()
    
    configs = []
    results = []
    
    # Load configurations
    if args.config1 and args.config2:
        # Load two specific configs
        if args.config1.endswith('.json'):
            config1 = ConfigLoader.load_json(args.config1)
        else:
            config1 = ConfigLoader.load_defconfig(args.config1)
        
        if args.config2.endswith('.json'):
            config2 = ConfigLoader.load_json(args.config2)
        else:
            config2 = ConfigLoader.load_defconfig(args.config2)
        
        configs = [config1, config2]
        results = [ComparisonEngine.compare(config1, config2)]
    
    elif args.dir:
        # Load from directory
        configs = ConfigLoader.load_directory(args.dir, args.pattern)
        
        if args.all:
            # Compare all configs
            for i in range(len(configs)):
                for j in range(i + 1, len(configs)):
                    results.append(ComparisonEngine.compare(configs[i], configs[j]))
        else:
            # Compare first two
            if len(configs) >= 2:
                results = [ComparisonEngine.compare(configs[0], configs[1])]
    
    else:
        print("⚠️  Please provide --config1 and --config2 or --dir")
        parser.print_help()
        sys.exit(1)
    
    if not configs:
        print("⚠️  No configurations loaded")
        sys.exit(1)
    
    if not results:
        print("⚠️  No comparisons generated")
        sys.exit(1)
    
    # Generate reports
    generator = ReportGenerator()
    
    if args.format in ['html', 'all']:
        html = generator.generate_html(results, configs)
        with open(f"{args.output}.html", 'w') as f:
            f.write(html)
        print(f"✅ HTML report saved: {args.output}.html")
    
    if args.format in ['markdown', 'all']:
        md = generator.generate_markdown(results)
        with open(f"{args.output}.md", 'w') as f:
            f.write(md)
        print(f"✅ Markdown report saved: {args.output}.md")
    
    if args.format in ['csv', 'all']:
        csv = generator.generate_csv(results)
        with open(f"{args.output}.csv", 'w') as f:
            f.write(csv)
        print(f"✅ CSV report saved: {args.output}.csv")
    
    # Print summary
    print("\n📊 Comparison Summary:")
    print(f"  Configurations: {len(configs)}")
    print(f"  Comparisons: {len(results)}")
    print(f"  Output: {args.output}.{args.format}")

if __name__ == "__main__":
    main()
