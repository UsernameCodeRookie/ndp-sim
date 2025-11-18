#!/usr/bin/env python3
"""
Convert the C++-generated VCD file to a proper VCD format using the pyvcd library.
"""

import sys
import re
from vcd import VCDWriter
from datetime import datetime

def parse_vcd_file(input_file):
    """Parse the existing VCD file to extract signals and value changes."""
    signals = {}  # signal_id -> (signal_name, width)
    changes = []  # (timestamp, signal_id, value)
    
    with open(input_file, 'r') as f:
        lines = f.readlines()
    
    in_definitions = False
    current_timestamp = 0
    
    for i, line in enumerate(lines):
        line = line.rstrip()
        
        # Skip header
        if line.startswith('$'):
            if line == '$enddefs':
                in_definitions = False
            elif line.startswith('$var'):
                # $var wire 32 a SCore-0_LSU_MEM_WRITE $end
                parts = line.split()
                if len(parts) >= 5:
                    width = int(parts[2])
                    signal_id = parts[3]
                    signal_name = parts[4]
                    signals[signal_id] = (signal_name, width)
            continue
        
        if not in_definitions and line and line[0] == '#':
            # Timestamp marker
            try:
                current_timestamp = int(line[1:])
            except:
                pass
        elif not in_definitions and line and not line.startswith('$'):
            # Value change line: "xxxxxxxxid" or "0id" or "1id"
            # Match pattern: hex/0/1 followed by signal id
            match = re.match(r'^([0-9a-fxXzZ\-?]+)([a-zA-Z0-9_]+)$', line)
            if match:
                value_str = match.group(1)
                signal_id = match.group(2)
                if signal_id in signals:
                    changes.append((current_timestamp, signal_id, value_str))
    
    return signals, changes

def create_vcd_with_library(signals, changes, output_file):
    """Create a proper VCD file using the vcd library."""
    
    # Group changes by timestamp
    changes_by_time = {}
    for ts, sig_id, value in changes:
        if ts not in changes_by_time:
            changes_by_time[ts] = []
        changes_by_time[ts].append((sig_id, value))
    
    with open(output_file, 'w') as f:
        # Create VCD writer
        with VCDWriter(f, timescale='1ps', date=datetime.now().strftime('%Y-%m-%d %H:%M:%S')) as writer:
            # Register all signals
            signal_vars = {}
            for sig_id, (sig_name, width) in sorted(signals.items()):
                var = writer.register_var('top', sig_name, 'wire', size=width)
                signal_vars[sig_id] = var
            
            # Write value changes
            for timestamp in sorted(changes_by_time.keys()):
                for sig_id, value_str in changes_by_time[timestamp]:
                    if sig_id in signal_vars:
                        var = signal_vars[sig_id]
                        # Convert value string to integer
                        try:
                            if value_str.startswith('0x') or value_str.startswith('0X'):
                                value = int(value_str, 16)
                            else:
                                value = int(value_str, 16)
                        except:
                            value = 0
                        
                        writer.change(var, timestamp, value)

def main():
    if len(sys.argv) < 2:
        print("Usage: convert_vcd.py <input_vcd> [output_vcd]")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else input_file.replace('.vcd', '_converted.vcd')
    
    print(f"Reading VCD from: {input_file}")
    signals, changes = parse_vcd_file(input_file)
    
    print(f"Found {len(signals)} signals and {len(changes)} value changes")
    
    print(f"Writing converted VCD to: {output_file}")
    create_vcd_with_library(signals, changes, output_file)
    
    print("Done!")

if __name__ == '__main__':
    main()
