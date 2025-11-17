import json

with open('programs/mac_example.json', 'r') as f:
    data = f.read()

# Find data_memory section
pos = data.find('"data_memory"')
if pos != -1:
    print(f"Found data_memory at position {pos}")
    print(f"Next 500 chars:\n{data[pos:pos+500]}")
else:
    print("data_memory not found")
