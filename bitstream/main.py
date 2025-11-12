#!/usr/bin/env python3
"""Command-line interface for bitstream generation and visualization."""

import argparse
import sys
import os
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bitstream.parse import (
    load_config, init_modules, build_entries, generate_bitstream,
    write_bitstream, dump_modules_to_binary, dump_modules_detailed, compare_bitstreams
)


def main():
    """Main entry point for bitstream CLI."""
    parser = argparse.ArgumentParser(
        description='Generate and analyze hardware bitstreams',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate bitstream with default config (detailed dump enabled by default)
  python -m bitstream.main
  
  # Use custom config and output directory
  python -m bitstream.main -c config.json -o ./output
  
  # Generate with comparison to reference
  python -m bitstream.main --compare data/bitstream.txt
  
  # Generate with visualizations
  python -m bitstream.main --visualize-binary --visualize-placement
  
  # Skip detailed dump for faster execution
  python -m bitstream.main --no-dump-detailed
  
  # Quiet mode (minimal output, detailed dump still generated)
  python -m bitstream.main -q
        """
    )
    
    # Input/Output arguments
    parser.add_argument(
        '-c', '--config',
        type=str,
        default='./data/gemm.json',
        help='Path to JSON configuration file (default: ./data/gemm.json)'
    )
    
    parser.add_argument(
        '-o', '--output-dir',
        type=str,
        default='./data',
        help='Output directory for generated files (default: ./data)'
    )
    
    # Output file names
    parser.add_argument(
        '--bitstream-name',
        type=str,
        default='generated_bitstream.txt',
        help='Name of generated bitstream file (default: generated_bitstream.txt)'
    )
    
    parser.add_argument(
        '--binary-name',
        type=str,
        default='modules_dump.bin',
        help='Name of binary dump file (default: modules_dump.bin)'
    )
    
    # Visualization options
    parser.add_argument(
        '--visualize-binary',
        action='store_true',
        help='Generate binary visualization output'
    )
    
    parser.add_argument(
        '--no-dump-detailed',
        action='store_true',
        help='Skip detailed field-by-field encoding dump (enabled by default)'
    )
    
    parser.add_argument(
        '--detailed-dump-output',
        type=str,
        default='detailed_dump.txt',
        help='Output filename for detailed dump (default: detailed_dump.txt)'
    )
    
    parser.add_argument(
        '--visualize-placement',
        action='store_true',
        help='Generate placement visualization (saves to placement.png)'
    )
    
    parser.add_argument(
        '--placement-output',
        type=str,
        default='placement.png',
        help='Output filename for placement visualization (default: placement.png)'
    )
    
    # Comparison and validation
    parser.add_argument(
        '--compare',
        type=str,
        metavar='REFERENCE_FILE',
        help='Compare generated bitstream with reference file (e.g., --compare data/bitstream.txt)'
    )
    
    # Config mask
    parser.add_argument(
        '--config-mask',
        type=str,
        default='11101110',
        help='8-bit config mask (default: 11101110 - IGA, SE, Buffer, Special enabled)'
    )
    
    # Output control
    parser.add_argument(
        '-q', '--quiet',
        action='store_true',
        help='Quiet mode - minimal output'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Verbose mode - detailed output'
    )
    
    args = parser.parse_args()
    
    # Create output directory if it doesn't exist
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Validate config mask
    if len(args.config_mask) != 8 or not all(c in '01' for c in args.config_mask):
        print(f"Error: Config mask must be 8 binary digits, got: {args.config_mask}")
        sys.exit(1)
    
    config_mask = [int(b) for b in args.config_mask]
    
    # Print configuration (unless quiet)
    if not args.quiet:
        print("="*80)
        print("BITSTREAM GENERATION CONFIGURATION")
        print("="*80)
        print(f"Config file:      {args.config}")
        print(f"Output directory: {args.output_dir}")
        print(f"Config mask:      {args.config_mask}")
        print(f"Binary dump:      {'Yes' if args.visualize_binary else 'No'}")
        print(f"Detailed dump:    {'No' if args.no_dump_detailed else 'Yes'}")
        print(f"Placement viz:    {'Yes' if args.visualize_placement else 'No'}")
        print(f"Compare:          {'Yes' if args.compare else 'No'}")
        if args.compare:
            print(f"Reference file:   {args.compare}")
        print("="*80)
    
    try:
        # Step 1: Load configuration
        if args.verbose:
            print(f"\n[1/6] Loading configuration from {args.config}...")
        cfg = load_config(args.config)
        
        # Step 2: Initialize modules
        if args.verbose:
            print("[2/6] Initializing modules and performing resource mapping...")
        modules = init_modules(cfg)
        
        # Step 3: Generate placement visualization (if requested)
        if args.visualize_placement:
            if args.verbose:
                print(f"[3/6] Generating placement visualization to {args.placement_output}...")
            from bitstream.config.mapper import NodeGraph, visualize_mapping
            mapper = NodeGraph.get().mapping
            connections = NodeGraph.get().connections
            placement_path = output_dir / args.placement_output
            visualize_mapping(mapper, connections, save_path=str(placement_path))
        elif args.verbose:
            print("[3/6] Skipping placement visualization")
        
        # Step 4: Generate binary dump (if requested)
        if args.visualize_binary:
            if args.verbose:
                print(f"[4/6] Generating binary dump to {args.binary_name}...")
            binary_path = output_dir / args.binary_name
            dump_modules_to_binary(modules, output_file=str(binary_path))
        elif args.verbose:
            print("[4/6] Skipping binary dump")
        
        # Step 4.5: Generate detailed dump (default enabled)
        if not args.no_dump_detailed:
            if args.verbose:
                print(f"[4.5/6] Generating detailed field dump to {args.detailed_dump_output}...")
            detailed_path = output_dir / args.detailed_dump_output
            dump_modules_detailed(modules, output_file=str(detailed_path))
        elif args.verbose:
            print("[4.5/6] Skipping detailed dump")
        
        # Step 5: Build bitstream entries and generate bitstream
        if args.verbose:
            print("[5/6] Building bitstream entries and generating bitstream...")
        entries = build_entries(modules)
        bitstream = generate_bitstream(entries, config_mask)
        
        bitstream_path = output_dir / args.bitstream_name
        write_bitstream(bitstream, output_file=str(bitstream_path))
        
        if not args.quiet:
            print(f"\n✓ Bitstream generated: {bitstream_path}")
            print(f"  Total size: {len(bitstream)} bits ({len(bitstream)//64} lines)")
        
        # Step 6: Compare with reference (if requested)
        if args.compare:
            if args.verbose:
                print(f"[6/6] Comparing with reference bitstream: {args.compare}...")
            
            if not Path(args.compare).exists():
                print(f"\nError: Reference file not found: {args.compare}")
                return 1
            else:
                result_info = {
                    'bitstream': bitstream,
                    'entries': entries,
                    'config_mask': config_mask,
                    'modules': modules,
                    'reference_file': args.compare
                }
                compare_bitstreams(result_info)
        elif args.verbose:
            print("[6/6] Skipping comparison (no --compare specified)")
        
        if not args.quiet:
            print("\n" + "="*80)
            print("✓ GENERATION COMPLETED SUCCESSFULLY")
            print("="*80)
        
        return 0
        
    except FileNotFoundError as e:
        print(f"\nError: File not found - {e}")
        return 1
    except Exception as e:
        print(f"\nError: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
