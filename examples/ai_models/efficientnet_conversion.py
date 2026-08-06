#!/usr/bin/env python3
"""
EfficientNet to RKNN Conversion Script
Converts EfficientNet PyTorch models to RKNN format for RK3568 NPU

Version: 1.0.0
Author: Sebastian
Date: 2024-08-06

Usage:
    python3 efficientnet_conversion.py --model efficientnet-b0.pt --output efficientnet-b0.rknn
    python3 efficientnet_conversion.py --model efficientnet-b0.pt --quantize INT8
    python3 efficientnet_conversion.py --help

Requirements:
    pip install torch torchvision onnx efficientnet-pytorch rknn-toolkit2
"""

import os
import sys
import argparse
import json
from pathlib import Path
import numpy as np
import torch
import torchvision.transforms as transforms
from PIL import Image
from rknn.api import RKNN

# ============================================================================
# Constants
# ============================================================================

IMAGENET_CLASSES = [f'class_{i}' for i in range(1000)]  # 1000 ImageNet classes

# EfficientNet variants
EFFICIENTNET_VARIANTS = {
    'b0': {'width': 1.0, 'depth': 1.0, 'resolution': 224, 'dropout': 0.2},
    'b1': {'width': 1.0, 'depth': 1.1, 'resolution': 240, 'dropout': 0.2},
    'b2': {'width': 1.1, 'depth': 1.2, 'resolution': 260, 'dropout': 0.3},
    'b3': {'width': 1.2, 'depth': 1.4, 'resolution': 300, 'dropout': 0.3},
    'b4': {'width': 1.4, 'depth': 1.8, 'resolution': 380, 'dropout': 0.4},
    'b5': {'width': 1.6, 'depth': 2.2, 'resolution': 456, 'dropout': 0.4},
}

COLORS = {
    'RED': '\033[91m',
    'GREEN': '\033[92m',
    'YELLOW': '\033[93m',
    'BLUE': '\033[94m',
    'MAGENTA': '\033[95m',
    'CYAN': '\033[96m',
    'RESET': '\033[0m'
}

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
# EfficientNet Model Class
# ============================================================================

class EfficientNetModel:
    """
    EfficientNet model wrapper for conversion to RKNN
    """
    
    def __init__(self, weights_path, variant='b0'):
        """
        Initialize EfficientNet model
        
        Args:
            weights_path: Path to weights file (.pt)
            variant: Model variant ('b0', 'b1', etc.)
        """
        self.weights_path = weights_path
        self.variant = variant
        self.model = None
        self.classes = 1000
        
        # Get model parameters
        params = EFFICIENTNET_VARIANTS.get(variant, EFFICIENTNET_VARIANTS['b0'])
        self.input_size = params['resolution']
        self.width = params['width']
        self.depth = params['depth']
        
    def load_model(self):
        """
        Load EfficientNet PyTorch model
        
        Returns:
            Loaded model
        """
        print_info(f"Loading EfficientNet-{self.variant} from: {self.weights_path}")
        
        try:
            # Import efficientnet
            from efficientnet_pytorch import EfficientNet
            
            # Load model
            self.model = EfficientNet.from_pretrained(
                f'efficientnet-{self.variant}',
                weights_path=self.weights_path
            )
            self.model.eval()
            
            print_success("Model loaded successfully")
            return self.model
            
        except Exception as e:
            print_error(f"Failed to load model: {e}")
            return None
    
    def preprocess_image(self, image_path):
        """
        Preprocess image for EfficientNet
        
        Args:
            image_path: Path to image file
            
        Returns:
            Preprocessed numpy array
        """
        # Load image
        image = Image.open(image_path).convert('RGB')
        
        # Transform
        transform = transforms.Compose([
            transforms.Resize((self.input_size, self.input_size)),
            transforms.ToTensor(),
            transforms.Normalize(mean=[0.485, 0.456, 0.406],
                               std=[0.229, 0.224, 0.225])
        ])
        
        # Apply transform
        tensor = transform(image)
        return tensor.numpy().astype(np.float32)
    
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
                self.model,
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
# RKNN Model Converter
# ============================================================================

class EfficientNetRKNNConverter:
    """
    RKNN converter for EfficientNet models
    """
    
    def __init__(self, target_platform='rk3568'):
        """
        Initialize RKNN converter
        
        Args:
            target_platform: Target platform
        """
        self.target_platform = target_platform
        self.rknn = RKNN()
        
    def convert(self, input_path, output_path, quantize='INT8', 
                dataset_path=None, do_quantization=True):
        """
        Convert model to RKNN format
        
        Args:
            input_path: Input model path (.pt or .onnx)
            output_path: Output RKNN path
            quantize: Quantization type
            dataset_path: Calibration dataset
            do_quantization: Enable quantization
            
        Returns:
            True on success, False on failure
        """
        print_header(f"Converting EfficientNet to RKNN")
        
        # Configuration
        quant_dtype_map = {
            'INT8': 'asymmetric_quantized-u8',
            'INT16': 'asymmetric_quantized-s16',
            'FP16': 'dynamic_fixed_point-16',
            'FP32': 'asymmetric_quantized-s8',
        }
        
        # Configure
        print_info("Configuring RKNN converter...")
        config = {
            'mean_values': [[123.675, 116.28, 103.53]],
            'std_values': [[58.395, 57.12, 57.375]],
            'target_platform': self.target_platform,
            'quantized_dtype': quant_dtype_map.get(quantize, 'asymmetric_quantized-u8'),
        }
        
        try:
            self.rknn.config(**config)
            print_success("Configuration applied")
        except Exception as e:
            print_error(f"Configuration failed: {e}")
            return False
        
        # Load model
        ext = Path(input_path).suffix.lower()
        
        if ext == '.onnx':
            print_info("Loading ONNX model...")
            ret = self.rknn.load_onnx(model=input_path)
        elif ext == '.pt' or ext == '.pth':
            print_info("Loading PyTorch model...")
            ret = self.rknn.load_pytorch(model=input_path)
        else:
            print_error(f"Unsupported format: {ext}")
            return False
        
        if ret != 0:
            print_error(f"Failed to load model: {ret}")
            return False
        print_success("Model loaded")
        
        # Build
        print_info("Building RKNN model...")
        build_params = {
            'do_quantization': do_quantization,
        }
        if dataset_path:
            build_params['dataset'] = dataset_path
        
        ret = self.rknn.build(**build_params)
        if ret != 0:
            print_error(f"Build failed: {ret}")
            return False
        print_success("Build successful")
        
        # Export
        print_info(f"Exporting RKNN model: {output_path}")
        ret = self.rknn.export_rknn(output_path)
        if ret != 0:
            print_error(f"Export failed: {ret}")
            return False
        print_success(f"RKNN model exported: {output_path}")
        
        return True
    
    def release(self):
        """Release RKNN resources"""
        self.rknn.release()

# ============================================================================
# Model Optimization
# ============================================================================

def optimize_efficientnet_for_rk3568(input_path, output_path, **kwargs):
    """
    Optimize EfficientNet for RK3568
    
    Args:
        input_path: Input model path
        output_path: Output RKNN path
        **kwargs: Optimization parameters
        
    Returns:
        True on success, False on failure
    """
    print_header("Optimizing EfficientNet for RK3568 NPU")
    
    # Create converter
    converter = EfficientNetRKNNConverter(target_platform='rk3568')
    
    # Convert
    success = converter.convert(
        input_path=input_path,
        output_path=output_path,
        quantize=kwargs.get('quantize', 'INT8'),
        dataset_path=kwargs.get('dataset', None),
        do_quantization=kwargs.get('quantize', 'INT8') != 'FP32'
    )
    
    # Cleanup
    converter.release()
    
    return success

# ============================================================================
# Model Verification
# ============================================================================

def verify_efficientnet_model(rknn_path, input_data):
    """
    Verify EfficientNet RKNN model
    
    Args:
        rknn_path: Path to RKNN model
        input_data: Input data for inference
        
    Returns:
        True on success, False on failure
    """
    print_header("Verifying EfficientNet RKNN Model")
    
    # Initialize RKNN
    rknn = RKNN()
    
    try:
        # Load model
        print_info("Loading RKNN model...")
        ret = rknn.load_rknn(rknn_path)
        if ret != 0:
            print_error(f"Failed to load RKNN: {ret}")
            return False
        
        # Initialize runtime
        print_info("Initializing runtime...")
        ret = rknn.init_runtime(target='rk3568')
        if ret != 0:
            print_error(f"Runtime init failed: {ret}")
            return False
        
        # Run inference
        print_info("Running inference...")
        outputs = rknn.inference(inputs=[input_data])
        
        if outputs is None:
            print_error("Inference failed")
            return False
        
        # Get predictions
        output = outputs[0]
        predicted_class = np.argmax(output)
        confidence = np.max(output)
        
        print_success("Inference completed")
        print_info(f"Predicted class: {predicted_class}")
        print_info(f"Confidence: {confidence:.2%}")
        
        return True
        
    except Exception as e:
        print_error(f"Verification failed: {e}")
        return False
    finally:
        rknn.release()

# ============================================================================
# Main Function
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='EfficientNet to RKNN Model Converter for RK3568'
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
        '--variant',
        choices=['b0', 'b1', 'b2', 'b3', 'b4', 'b5'],
        default='b0',
        help='EfficientNet variant (default: b0)'
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
        '--image',
        help='Test image for verification'
    )
    
    args = parser.parse_args()
    
    # Print header
    print_header("EfficientNet to RKNN Model Converter")
    print_info(f"Model: {args.model}")
    print_info(f"Variant: {args.variant}")
    print_info(f"Output: {args.output}")
    print_info(f"Quantization: {args.quantize}")
    
    # Check input file
    if not os.path.exists(args.model):
        print_error(f"Model file not found: {args.model}")
        sys.exit(1)
    
    # Convert model
    success = optimize_efficientnet_for_rk3568(
        input_path=args.model,
        output_path=args.output,
        quantize=args.quantize,
        dataset=args.dataset
    )
    
    if not success:
        print_error("Model conversion failed")
        sys.exit(1)
    
    # Verify if requested
    if args.verify:
        # Create test input
        test_input = np.random.rand(1, 3, 224, 224).astype(np.float32)
        
        # If image provided, use it
        if args.image:
            print_info(f"Using test image: {args.image}")
            model = EfficientNetModel(args.model, args.variant)
            test_input = model.preprocess_image(args.image)
        
        if not verify_efficientnet_model(args.output, test_input):
            print_warning("Model verification failed or produced warnings")
    
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
