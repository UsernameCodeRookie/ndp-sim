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
  # Generate bitstream with default config (binary and detailed dumps enabled by default)
  python -m bitstream.main
  
  # Use custom config and output directory
  python -m bitstream.main -c config.json -o ./output
  
  # Generate with comparison to reference
  python -m bitstream.main --compare data/bitstream.txt
  
  # Generate with placement visualization
  python -m bitstream.main --visualize-placement
  
  # Use direct mapping mode (no constraint search)
  python -m bitstream.main --direct-mapping
  
  # Use heuristic search (simulated annealing) for large graphs
  python -m bitstream.main --heuristic-search --heuristic-iterations 10000
  
  # Skip dumps for faster execution
  python -m bitstream.main --no-dump-binary --no-dump-detailed
  
  # Quiet mode (minimal output, dumps still generated)
  python -m bitstream.main -q
        """
    )
    # Set default behavior: heuristic search enabled by default
    parser.set_defaults(heuristic_search=True)
    
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
    
    # Dump options
    parser.add_argument(
        '--no-dump-binary',
        action='store_true',
        help='Skip binary dump generation (enabled by default)'
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
        '--no-dump-parsed',
        action='store_true',
        help='Skip parsed bitstream generation (enabled by default)'
    )
    
    parser.add_argument(
        '--parsed-name',
        type=str,
        default='parsed_bitstream.txt',
        help='Name of parsed bitstream file (default: parsed_bitstream.txt)'
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
    
    # Mapping mode
    parser.add_argument(
        '--direct-mapping',
        action='store_true',
        help='Use direct logical→physical index mapping without constraint search'
    )
    
    # Heuristic search enabled by default; use --no-heuristic-search to disable
    parser.add_argument(
        '--heuristic-search',
        action='store_true',
        dest='heuristic_search',
        help='Enable heuristic search (simulated annealing) for large graphs (enabled by default)',
    )
    parser.add_argument(
        '--no-heuristic-search',
        action='store_false',
        dest='heuristic_search',
        help='Disable heuristic search (use when you want direct mapping or custom behavior)'
    )
    
    parser.add_argument(
        '--heuristic-iterations',
        type=int,
        default=5000,
        help='Maximum iterations for heuristic search (default: 5000)'
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
    
    # Validate mutually exclusive mapping modes
    if args.direct_mapping and args.heuristic_search:
        print("Error: Cannot use both --direct-mapping and --heuristic-search simultaneously")
        sys.exit(1)
    
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
        if args.direct_mapping:
            mapping_mode = "Direct (no search)"
        elif args.heuristic_search:
            mapping_mode = f"Heuristic (simulated annealing, {args.heuristic_iterations} iters)"
        else:
            mapping_mode = "Heuristic (simulated annealing - fallback)"
        print(f"Mapping mode:     {mapping_mode}")
        print(f"Binary dump:      {'No' if args.no_dump_binary else 'Yes'}")
        print(f"Detailed dump:    {'No' if args.no_dump_detailed else 'Yes'}")
        print(f"Parsed dump:      {'No' if args.no_dump_parsed else 'Yes'}")
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

        modules = init_modules(cfg, 
                             use_direct_mapping=args.direct_mapping,
                             use_heuristic_search=args.heuristic_search,
                             heuristic_iterations=args.heuristic_iterations)
        
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
        
        # Step 4: Generate binary dump (default enabled)
        if not args.no_dump_binary:
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
        write_bitstream(entries, output_file=str(bitstream_path))
        
        # Step 5.5: Generate parsed bitstream (default enabled)
        if not args.no_dump_parsed:
            if args.verbose:
                print(f"[5.5/6] Generating parsed bitstream to {args.parsed_name}...")
            parsed_path = output_dir / args.parsed_name
            write_bitstream(entries, output_file=str(parsed_path))
        elif args.verbose:
            print("[5.5/6] Skipping parsed bitstream")
        
        if not args.quiet:
            print(f"\n✓ Bitstream generated: {bitstream_path}")
            if not args.no_dump_binary:
                print(f"✓ Binary dump: {output_dir / args.binary_name}")
            if not args.no_dump_detailed:
                print(f"✓ Detailed dump: {output_dir / args.detailed_dump_output}")
            if not args.no_dump_parsed:
                print(f"✓ Parsed bitstream: {output_dir / args.parsed_name}")
        
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
