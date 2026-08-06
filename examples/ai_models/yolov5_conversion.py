#!/usr/bin/env python3
"""
YOLOv5 to RKNN Conversion Script
Converts YOLOv5 PyTorch models to RKNN format for RK3568 NPU

Version: 1.0.0
Author: Sebastian
Date: 2024-08-06

Usage:
    python3 yolov5_conversion.py --model yolov5s.pt --output yolov5s.rknn
    python3 yolov5_conversion.py --model yolov5s.pt --quantize INT8
    python3 yolov5_conversion.py --help

Requirements:
    pip install torch torchvision onnx rknn-toolkit2
"""

import os
import sys
import argparse
import time
import json
from pathlib import Path
import numpy as np
import cv2
import torch
import torchvision
from rknn.api import RKNN

# ============================================================================
# Constants
# ============================================================================

COLORS = {
    'RED': '\033[91m',
    'GREEN': '\033[92m',
    'YELLOW': '\033[93m',
    'BLUE': '\033[94m',
    'MAGENTA': '\033[95m',
    'CYAN': '\033[96m',
    'RESET': '\033[0m'
}

CLASS_NAMES = [
    'person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus', 'train',
    'truck', 'boat', 'traffic light', 'fire hydrant', 'stop sign',
    'parking meter', 'bench', 'bird', 'cat', 'dog', 'horse', 'sheep',
    'cow', 'elephant', 'bear', 'zebra', 'giraffe', 'backpack', 'umbrella',
    'handbag', 'tie', 'suitcase', 'frisbee', 'skis', 'snowboard',
    'sports ball', 'kite', 'baseball bat', 'baseball glove', 'skateboard',
    'surfboard', 'tennis racket', 'bottle', 'wine glass', 'cup', 'fork',
    'knife', 'spoon', 'bowl', 'banana', 'apple', 'sandwich', 'orange',
    'broccoli', 'carrot', 'hot dog', 'pizza', 'donut', 'cake', 'chair',
    'couch', 'potted plant', 'bed', 'dining table', 'toilet', 'tv',
    'laptop', 'mouse', 'remote', 'keyboard', 'cell phone', 'microwave',
    'oven', 'toaster', 'sink', 'refrigerator', 'book', 'clock', 'vase',
    'scissors', 'teddy bear', 'hair drier', 'toothbrush'
]

# ============================================================================
# Helper Functions
# ============================================================================

def print_info(msg):
    print(f"{COLORS['CYAN']}[INFO]{COLORS['RESET']} {msg}")

def print_success(msg):
    print(f"{COLORS['GREEN']}[SUCCESS]{COLORS['RESET']} {msg}")

def print_error(msg):
    print(f"{COLORS['RED']}[ERROR]{COLORS['RESET']} {msg}")

def print_warning(msg):
    print(f"{COLORS['YELLOW']}[WARNING]{COLORS['RESET']} {msg}")

def print_header(title):
    print(f"\n{COLORS['BLUE']}{'='*60}{COLORS['RESET']}")
    print(f"{COLORS['BLUE']}  {title}{COLORS['RESET']}")
    print(f"{COLORS['BLUE']}{'='*60}{COLORS['RESET']}")

# ============================================================================
# YOLOv5 Model Class
# ============================================================================

class YOLOv5Model:
    """
    YOLOv5 model wrapper for conversion to RKNN
    """
    
    def __init__(self, weights_path):
        """
        Initialize YOLOv5 model
        
        Args:
            weights_path: Path to YOLOv5 weights file (.pt)
        """
        self.weights_path = weights_path
        self.model = None
        self.input_size = 640
        self.classes = 80
        
    def load_model(self):
        """
        Load YOLOv5 PyTorch model
        
        Returns:
            Loaded model
        """
        print_info(f"Loading YOLOv5 model from: {self.weights_path}")
        
        # Load model
        try:
            # Dynamic import of YOLOv5
            self.model = torch.hub.load(
                'ultralytics/yolov5', 
                'custom', 
                path=self.weights_path,
                force_reload=False
            )
            print_success("Model loaded successfully")
            return self.model
            
        except Exception as e:
            print_error(f"Failed to load model: {e}")
            return None
    
    def export_to_onnx(self, output_path, batch_size=1):
        """
        Export PyTorch model to ONNX format
        
        Args:
            output_path: Output ONNX file path
            batch_size: Batch size for export
            
        Returns:
            True on success, False on failure
        """
        print_info(f"Exporting to ONNX: {output_path}")
        
        if self.model is None:
            if not self.load_model():
                return False
        
        # Set model to evaluation mode
        self.model.eval()
        
        # Create dummy input
        dummy_input = torch.randn(batch_size, 3, self.input_size, self.input_size)
        
        # Export to ONNX
        try:
            torch.onnx.export(
                self.model.model,
                dummy_input,
                output_path,
                export_params=True,
                opset_version=11,
                do_constant_folding=True,
                input_names=['input'],
                output_names=['output'],
                dynamic_axes={
                    'input': {0: 'batch_size'},
                    'output': {0: 'batch_size'}
                }
            )
            print_success(f"ONNX export successful: {output_path}")
            return True
            
        except Exception as e:
            print_error(f"ONNX export failed: {e}")
            return False

# ============================================================================
# RKNN Converter Class
# ============================================================================

class RKNNConverter:
    """
    RKNN model converter for RK3568 NPU
    """
    
    def __init__(self, target_platform='rk3568'):
        """
        Initialize RKNN converter
        
        Args:
            target_platform: Target platform ('rk3568', 'rk3588', etc.)
        """
        self.target_platform = target_platform
        self.rknn = RKNN()
        self.model_path = None
        self.rknn_path = None
        
    def configure(self, **kwargs):
        """
        Configure RKNN conversion parameters
        
        Args:
            **kwargs: Configuration parameters
        """
        print_info("Configuring RKNN converter...")
        
        # Default configuration
        config = {
            'mean_values': [[0, 0, 0]],
            'std_values': [[255, 255, 255]],
            'target_platform': self.target_platform,
            'quantized_dtype': 'asymmetric_quantized-u8',
        }
        
        # Update with provided kwargs
        config.update(kwargs)
        
        # Apply configuration
        try:
            self.rknn.config(**config)
            print_success("Configuration applied")
        except Exception as e:
            print_error(f"Configuration failed: {e}")
            return False
            
        return True
    
    def load_onnx(self, model_path):
        """
        Load ONNX model
        
        Args:
            model_path: Path to ONNX model
            
        Returns:
            True on success, False on failure
        """
        print_info(f"Loading ONNX model: {model_path}")
        
        try:
            ret = self.rknn.load_onnx(model=model_path)
            if ret != 0:
                print_error(f"Failed to load ONNX: {ret}")
                return False
            self.model_path = model_path
            print_success("ONNX model loaded")
            return True
            
        except Exception as e:
            print_error(f"ONNX load failed: {e}")
            return False
    
    def load_torch(self, model_path):
        """
        Load PyTorch model directly
        
        Args:
            model_path: Path to PyTorch model
            
        Returns:
            True on success, False on failure
        """
        print_info(f"Loading PyTorch model: {model_path}")
        
        try:
            ret = self.rknn.load_pytorch(model=model_path)
            if ret != 0:
                print_error(f"Failed to load PyTorch: {ret}")
                return False
            self.model_path = model_path
            print_success("PyTorch model loaded")
            return True
            
        except Exception as e:
            print_error(f"PyTorch load failed: {e}")
            return False
    
    def build(self, do_quantization=True, dataset_path=None, **kwargs):
        """
        Build RKNN model
        
        Args:
            do_quantization: Enable quantization
            dataset_path: Path to calibration dataset
            **kwargs: Additional build parameters
            
        Returns:
            True on success, False on failure
        """
        print_info("Building RKNN model...")
        
        build_params = {
            'do_quantization': do_quantization,
            'dataset': dataset_path,
        }
        build_params.update(kwargs)
        
        try:
            ret = self.rknn.build(**build_params)
            if ret != 0:
                print_error(f"Build failed: {ret}")
                return False
            print_success("RKNN model built successfully")
            return True
            
        except Exception as e:
            print_error(f"Build failed: {e}")
            return False
    
    def export(self, output_path):
        """
        Export RKNN model to file
        
        Args:
            output_path: Output RKNN file path
            
        Returns:
            True on success, False on failure
        """
        print_info(f"Exporting RKNN model: {output_path}")
        
        try:
            ret = self.rknn.export_rknn(output_path)
            if ret != 0:
                print_error(f"Export failed: {ret}")
                return False
            self.rknn_path = output_path
            print_success(f"RKNN model exported: {output_path}")
            return True
            
        except Exception as e:
            print_error(f"Export failed: {e}")
            return False
    
    def init_runtime(self, target=None, device_id=None):
        """
        Initialize RKNN runtime for inference
        
        Args:
            target: Target device
            device_id: Device ID
            
        Returns:
            True on success, False on failure
        """
        print_info("Initializing RKNN runtime...")
        
        try:
            ret = self.rknn.init_runtime(
                target=target,
                device_id=device_id
            )
            if ret != 0:
                print_error(f"Runtime init failed: {ret}")
                return False
            print_success("Runtime initialized")
            return True
            
        except Exception as e:
            print_error(f"Runtime init failed: {e}")
            return False
    
    def inference(self, input_data):
        """
        Run inference with RKNN model
        
        Args:
            input_data: Input data (numpy array)
            
        Returns:
            Output data or None on failure
        """
        try:
            outputs = self.rknn.inference(inputs=[input_data])
            return outputs
        except Exception as e:
            print_error(f"Inference failed: {e}")
            return None
    
    def release(self):
        """Release RKNN resources"""
        print_info("Releasing RKNN resources...")
        self.rknn.release()
        print_success("Resources released")

# ============================================================================
# Model Optimization Functions
# ============================================================================

def optimize_model_for_rk3568(converter, input_path, output_path, **kwargs):
    """
    Optimize model for RK3568 NPU
    
    Args:
        converter: RKNNConverter instance
        input_path: Input model path
        output_path: Output RKNN path
        **kwargs: Optimization parameters
        
    Returns:
        True on success, False on failure
    """
    print_header("Optimizing Model for RK3568")
    
    # Configure converter
    config = {
        'mean_values': [[0, 0, 0]],
        'std_values': [[255, 255, 255]],
        'target_platform': 'rk3568',
        'quantized_dtype': 'asymmetric_quantized-u8',
    }
    config.update(kwargs)
    
    if not converter.configure(**config):
        return False
    
    # Load model based on extension
    ext = Path(input_path).suffix.lower()
    
    if ext == '.onnx':
        if not converter.load_onnx(input_path):
            return False
    elif ext == '.pt' or ext == '.pth':
        if not converter.load_torch(input_path):
            return False
    else:
        print_error(f"Unsupported model format: {ext}")
        return False
    
    # Build with quantization
    do_quantize = kwargs.get('quantize', True)
    dataset_path = kwargs.get('dataset', None)
    
    if not converter.build(
        do_quantization=do_quantize,
        dataset_path=dataset_path
    ):
        return False
    
    # Export RKNN
    if not converter.export(output_path):
        return False
    
    print_success(f"Model optimized and saved to: {output_path}")
    return True

# ============================================================================
# Model Verification Functions
# ============================================================================

def verify_rknn_model(rknn_path, input_data, expected_output=None):
    """
    Verify RKNN model inference
    
    Args:
        rknn_path: Path to RKNN model
        input_data: Input data for inference
        expected_output: Expected output for comparison
        
    Returns:
        True on success, False on failure
    """
    print_header("Verifying RKNN Model")
    
    # Create converter for verification
    converter = RKNNConverter()
    
    # Configure and load
    if not converter.configure(target_platform='rk3568'):
        return False
    
    try:
        # Load RKNN model
        ret = converter.rknn.load_rknn(rknn_path)
        if ret != 0:
            print_error(f"Failed to load RKNN: {ret}")
            return False
        
        # Initialize runtime
        if not converter.init_runtime():
            return False
        
        # Run inference
        outputs = converter.inference(input_data)
        if outputs is None:
            return False
        
        print_success("Inference completed successfully")
        
        # Verify if expected output provided
        if expected_output is not None:
            # Compare outputs
            for i, (out, exp) in enumerate(zip(outputs, expected_output)):
                diff = np.abs(out - exp).max()
                print_info(f"Output {i} diff: {diff}")
                if diff > 1e-3:
                    print_warning(f"Output {i} differs from expected")
        
        converter.release()
        return True
        
    except Exception as e:
        print_error(f"Verification failed: {e}")
        converter.release()
        return False

# ============================================================================
# Main Function
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='YOLOv5 to RKNN Model Converter for RK3568'
    )
    
    parser.add_argument(
        '--model',
        required=True,
        help='Input model file (.pt, .onnx)'
    )
    
    parser.add_argument(
        '--output',
        required=True,
        help='Output RKNN file (.rknn)'
    )
    
    parser.add_argument(
        '--quantize',
        choices=['INT8', 'INT16', 'FP16', 'FP32'],
        default='INT8',
        help='Quantization type (default: INT8)'
    )
    
    parser.add_argument(
        '--dataset',
        help='Calibration dataset path for quantization'
    )
    
    parser.add_argument(
        '--verify',
        action='store_true',
        help='Verify the converted model'
    )
    
    parser.add_argument(
        '--input-size',
        type=int,
        default=640,
        help='Input size (default: 640)'
    )
    
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Verbose output'
    )
    
    args = parser.parse_args()
    
    # Print header
    print_header("YOLOv5 to RKNN Model Converter")
    print_info(f"Model: {args.model}")
    print_info(f"Output: {args.output}")
    print_info(f"Quantization: {args.quantize}")
    print_info(f"Input Size: {args.input_size}")
    
    # Set quantization dtype
    quant_dtype_map = {
        'INT8': 'asymmetric_quantized-u8',
        'INT16': 'asymmetric_quantized-s16',
        'FP16': 'dynamic_fixed_point-16',
        'FP32': 'asymmetric_quantized-s8',
    }
    
    # Create converter
    converter = RKNNConverter(target_platform='rk3568')
    
    # Configure
    config = {
        'mean_values': [[0, 0, 0]],
        'std_values': [[255, 255, 255]],
        'quantized_dtype': quant_dtype_map.get(args.quantize, 'asymmetric_quantized-u8'),
    }
    
    if not converter.configure(**config):
        print_error("Configuration failed")
        sys.exit(1)
    
    # Load model
    ext = Path(args.model).suffix.lower()
    if ext == '.onnx':
        success = converter.load_onnx(args.model)
    elif ext == '.pt' or ext == '.pth':
        success = converter.load_torch(args.model)
    else:
        print_error(f"Unsupported model format: {ext}")
        sys.exit(1)
    
    if not success:
        print_error("Model loading failed")
        sys.exit(1)
    
    # Build RKNN
    if not converter.build(
        do_quantization=(args.quantize != 'FP32'),
        dataset_path=args.dataset
    ):
        print_error("Build failed")
        sys.exit(1)
    
    # Export
    if not converter.export(args.output):
        print_error("Export failed")
        sys.exit(1)
    
    # Verify if requested
    if args.verify:
        # Create test input
        test_input = np.random.rand(1, 3, args.input_size, args.input_size).astype(np.float32)
        if not verify_rknn_model(args.output, test_input):
            print_warning("Model verification failed or produced warnings")
    
    # Cleanup
    converter.release()
    
    print_header("Conversion Complete")
    print_success(f"RKNN model saved to: {args.output}")
    
    # Print model info
    model_size = os.path.getsize(args.output)
    print_info(f"Model size: {model_size / 1024 / 1024:.2f} MB")
    
    print_info("\nNext steps:")
    print_info("  1. Copy model to target device")
    print_info("  2. Run inference with RKNN runtime")
    print_info("  3. Test with sample images")

if __name__ == "__main__":
    main()
