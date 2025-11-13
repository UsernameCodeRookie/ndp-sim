#!/usr/bin/env python3
"""
Test script to verify relative index calculation in Connect class.
"""

import sys
sys.path.insert(0, '/Users/chisato/Desktop/projects/ndp-sim')

# Import the main module to initialize everything
import bitstream.main as main_module

# Now import after initialization
from bitstream.index import NodeIndex, Connect

def test_lc_to_lc():
    """Test LC → LC relative indexing: [i-2, i-1, i+1, i+2] → [0, 1, 2, 3]"""
    print("\n=== Testing LC → LC ===")
    
    # Create destination LC
    dst_lc = NodeIndex("DRAM_LC.LC3")
    dst_lc._physical_id = 3
    
    # Test connections from different source LCs
    test_cases = [
        ("DRAM_LC.LC1", 1, 0),  # i-2 → 0
        ("DRAM_LC.LC2", 2, 1),  # i-1 → 1
        ("DRAM_LC.LC4", 4, 2),  # i+1 → 2
        ("DRAM_LC.LC5", 5, 3),  # i+2 → 3
    ]
    
    for src_name, src_id, expected in test_cases:
        src = NodeIndex(src_name)
        src._physical_id = src_id
        conn = Connect(src_name, dst_lc)
        result = int(conn)
        status = "✓" if result == expected else "✗"
        print(f"{status} {src_name} (id={src_id}) → {dst_lc.node_name} (id={dst_lc._physical_id}): {result} (expected {expected})")

def test_lc_to_row_lc():
    """Test LC → ROW_LC: LC [(group_id-1)*2 : (group_id+1)*2+1] → [0, 1, 2, 3, 4, 5]"""
    print("\n=== Testing LC → ROW_LC (GROUP) ===")
    
    # GROUP1.ROW_LC (group_id=1), expects LC from range [(1-1)*2 : (1+1)*2+1] = [0, 1, 2, 3, 4, 5]
    dst_row_lc = NodeIndex("GROUP1.ROW_LC")
    dst_row_lc._physical_id = 1  # Shares GROUP's physical_id
    
    test_cases = [
        ("DRAM_LC.LC0", 0, 0),
        ("DRAM_LC.LC1", 1, 1),
        ("DRAM_LC.LC2", 2, 2),
        ("DRAM_LC.LC3", 3, 3),
        ("DRAM_LC.LC4", 4, 4),
        ("DRAM_LC.LC5", 5, 5),
    ]
    
    for src_name, src_id, expected in test_cases:
        src = NodeIndex(src_name)
        src._physical_id = src_id
        conn = Connect(src_name, dst_row_lc)
        result = int(conn)
        status = "✓" if result == expected else "✗"
        print(f"{status} {src_name} (id={src_id}) → {dst_row_lc.node_name} (group_id={dst_row_lc._physical_id}): {result} (expected {expected})")

def test_row_lc_to_col_lc():
    """Test ROW_LC → COL_LC: should always return 6"""
    print("\n=== Testing ROW_LC → COL_LC ===")
    
    src_row_lc = NodeIndex("GROUP0.ROW_LC")
    src_row_lc._physical_id = 0
    dst_col_lc = NodeIndex("GROUP0.COL_LC")
    dst_col_lc._physical_id = 0
    
    conn = Connect("GROUP0.ROW_LC", dst_col_lc)
    result = int(conn)
    expected = 6
    status = "✓" if result == expected else "✗"
    print(f"{status} GROUP0.ROW_LC → GROUP0.COL_LC: {result} (expected {expected})")

def test_lc_to_pe():
    """Test LC → PE: [i-1, i, i+1] → [0, 1, 2]"""
    print("\n=== Testing LC → PE ===")
    
    # PE2 expects connections from LC1, LC2, LC3
    dst_pe = NodeIndex("LC_PE.PE2")
    dst_pe._physical_id = 2
    
    test_cases = [
        ("DRAM_LC.LC1", 1, 0),  # i-1 → 0
        ("DRAM_LC.LC2", 2, 1),  # i → 1
        ("DRAM_LC.LC3", 3, 2),  # i+1 → 2
    ]
    
    for src_name, src_id, expected in test_cases:
        src = NodeIndex(src_name)
        src._physical_id = src_id
        conn = Connect(src_name, dst_pe)
        result = int(conn)
        status = "✓" if result == expected else "✗"
        print(f"{status} {src_name} (id={src_id}) → {dst_pe.node_name} (id={dst_pe._physical_id}): {result} (expected {expected})")

def test_pe_to_pe():
    """Test PE → PE: [i-2, i-1, i+1, i+2] → [3, 4, 5, 6]"""
    print("\n=== Testing PE → PE ===")
    
    dst_pe = NodeIndex("LC_PE.PE3")
    dst_pe._physical_id = 3
    
    test_cases = [
        ("LC_PE.PE1", 1, 3),  # i-2 → 3
        ("LC_PE.PE2", 2, 4),  # i-1 → 4
        ("LC_PE.PE4", 4, 5),  # i+1 → 5
        ("LC_PE.PE5", 5, 6),  # i+2 → 6
    ]
    
    for src_name, src_id, expected in test_cases:
        src = NodeIndex(src_name)
        src._physical_id = src_id
        conn = Connect(src_name, dst_pe)
        result = int(conn)
        status = "✓" if result == expected else "✗"
        print(f"{status} {src_name} (id={src_id}) → {dst_pe.node_name} (id={dst_pe._physical_id}): {result} (expected {expected})")

def test_lc_to_stream():
    """Test LC → STREAM: LC [(stream_id-1)*2 : (stream_id+1)*2+1] → [0, 1, 2, 3, 4, 5]"""
    print("\n=== Testing LC → STREAM ===")
    
    # STREAM1 expects LC from range [(1-1)*2 : (1+1)*2+1] = [0, 1, 2, 3, 4, 5]
    dst_stream = NodeIndex("STREAM.stream1")
    dst_stream._physical_id = 1
    
    test_cases = [
        ("DRAM_LC.LC0", 0, 0),
        ("DRAM_LC.LC1", 1, 1),
        ("DRAM_LC.LC2", 2, 2),
        ("DRAM_LC.LC3", 3, 3),
        ("DRAM_LC.LC4", 4, 4),
        ("DRAM_LC.LC5", 5, 5),
    ]
    
    for src_name, src_id, expected in test_cases:
        src = NodeIndex(src_name)
        src._physical_id = src_id
        conn = Connect(src_name, dst_stream)
        result = int(conn)
        status = "✓" if result == expected else "✗"
        print(f"{status} {src_name} (id={src_id}) → {dst_stream.node_name} (id={dst_stream._physical_id}): {result} (expected {expected})")

def test_pe_to_stream():
    """Test PE → STREAM: PE [(stream_id-1)*2 : (stream_id+1)*2+1] → [6, 7, 8, 9, 10, 11]"""
    print("\n=== Testing PE → STREAM ===")
    
    # STREAM1 expects PE from range [(1-1)*2 : (1+1)*2+1] = [0, 1, 2, 3, 4, 5]
    dst_stream = NodeIndex("STREAM.stream1")
    dst_stream._physical_id = 1
    
    test_cases = [
        ("LC_PE.PE0", 0, 6),   # PE0 → 6
        ("LC_PE.PE1", 1, 7),   # PE1 → 7
        ("LC_PE.PE2", 2, 8),   # PE2 → 8
        ("LC_PE.PE3", 3, 9),   # PE3 → 9
        ("LC_PE.PE4", 4, 10),  # PE4 → 10
        ("LC_PE.PE5", 5, 11),  # PE5 → 11
    ]
    
    for src_name, src_id, expected in test_cases:
        src = NodeIndex(src_name)
        src._physical_id = src_id
        conn = Connect(src_name, dst_stream)
        result = int(conn)
        status = "✓" if result == expected else "✗"
        print(f"{status} {src_name} (id={src_id}) → {dst_stream.node_name} (id={dst_stream._physical_id}): {result} (expected {expected})")

if __name__ == "__main__":
    print("=" * 60)
    print("Testing Relative Index Calculation in Connect Class")
    print("=" * 60)
    
    test_lc_to_lc()
    test_lc_to_row_lc()
    test_row_lc_to_col_lc()
    test_lc_to_pe()
    test_pe_to_pe()
    test_lc_to_stream()
    test_pe_to_stream()
    
    print("\n" + "=" * 60)
    print("All tests completed!")
    print("=" * 60)
