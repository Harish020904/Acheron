#!/usr/bin/env python3
"""Utility script to save raw Melbourne OSM data from the fetched content."""
import json, os

# Read the raw content file
src = r'C:\Users\Harish\.gemini\antigravity\brain\5ac974f7-1f3c-42df-bd89-cebbc516c166\.system_generated\steps\594\content.md'
with open(src, 'r', encoding='utf-8') as f:
    content = f.read()

# Find JSON start
lines = content.split('\n')
json_start = 0
for i, line in enumerate(lines):
    if line.strip().startswith('{'):
        json_start = i
        break

json_text = '\n'.join(lines[json_start:])
data = json.loads(json_text)

element_count = len(data['elements'])
print(f'Valid JSON with {element_count} elements')

os.makedirs('tools', exist_ok=True)
with open('tools/melbourne_osm_raw.json', 'w', encoding='utf-8') as f:
    json.dump(data, f)
print('Saved to tools/melbourne_osm_raw.json')
