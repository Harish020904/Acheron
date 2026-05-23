#!/usr/bin/env python3
"""
Melbourne OSM Data Processor for Project Acheron
Fetches Melbourne CBD road data from Overpass API and converts to
a compact C++ header file for use in the simulation.

Usage: python process_melbourne_osm.py
Output: ../../assets/data/melbourne_map_data.h
"""

import json
import math
import sys
import urllib.request
import urllib.parse

# ============================================================
# Melbourne CBD Bounding Box
# ============================================================
LAT_MIN = -37.825
LAT_MAX = -37.788
LON_MIN = 144.935
LON_MAX = 144.985

# World coordinate scale (pixels)
WORLD_WIDTH  = 8000.0
WORLD_HEIGHT = 6500.0

# Road type priority (lower = more important, thicker)
ROAD_CLASS_MAP = {
    "motorway":       0,
    "motorway_link":  0,
    "trunk":          1,
    "trunk_link":     1,
    "primary":        2,
    "primary_link":   2,
    "secondary":      3,
    "secondary_link": 3,
    "tertiary":       4,
    "tertiary_link":  4,
    "residential":    5,
    "living_street":  5,
    "unclassified":   5,
    "service":        6,
    "footway":        7,
    "cycleway":       7,
    "path":           7,
    "pedestrian":     7,
}

def lat_lon_to_world(lat, lon):
    """Simple linear projection of lat/lon to world pixel coordinates."""
    x = (lon - LON_MIN) / (LON_MAX - LON_MIN) * WORLD_WIDTH
    # Invert Y: higher lat = lower y on screen (north is up)
    y = (1.0 - (lat - LAT_MIN) / (LAT_MAX - LAT_MIN)) * WORLD_HEIGHT
    return x, y

def fetch_osm_data():
    """Fetch Melbourne CBD road network from Overpass API."""
    query = f"""[out:json][timeout:45];
(
  way["highway"~"^(motorway|trunk|primary|secondary|tertiary|residential|living_street|unclassified)$"]
    ({LAT_MIN},{LON_MIN},{LAT_MAX},{LON_MAX});
  >;
);
out body;"""
    
    encoded_query = urllib.parse.quote(query)
    url = f"https://overpass-api.de/api/interpreter?data={encoded_query}"
    
    print(f"Fetching Melbourne road data from Overpass API...")
    try:
        with urllib.request.urlopen(url, timeout=60) as response:
            data = response.read().decode('utf-8')
            print(f"Received {len(data)} bytes")
            return json.loads(data)
    except Exception as e:
        print(f"Error fetching data: {e}")
        return None

def process_osm_data(osm_data):
    """Extract nodes and edges from raw OSM data."""
    nodes = {}   # id -> (world_x, world_y)
    edges = []   # (from_id, to_id, road_class)
    
    for element in osm_data.get("elements", []):
        etype = element.get("type")
        
        if etype == "node":
            nid = element["id"]
            lat = element["lat"]
            lon = element["lon"]
            # Skip nodes outside our bounds
            if LAT_MIN <= lat <= LAT_MAX and LON_MIN <= lon <= LON_MAX:
                wx, wy = lat_lon_to_world(lat, lon)
                nodes[nid] = (wx, wy)
        
        elif etype == "way":
            tags = element.get("tags", {})
            highway = tags.get("highway", "")
            road_class = ROAD_CLASS_MAP.get(highway, -1)
            if road_class < 0:
                continue
            
            way_nodes = element.get("nodes", [])
            # Create edges between consecutive nodes
            for i in range(len(way_nodes) - 1):
                from_id = way_nodes[i]
                to_id   = way_nodes[i + 1]
                edges.append((from_id, to_id, road_class))
    
    return nodes, edges

def remap_ids(nodes, edges):
    """Remap large OSM IDs to sequential 0-based indices."""
    # Filter edges to only those where both nodes exist
    valid_node_ids = set(nodes.keys())
    valid_edges = [(f, t, rc) for f, t, rc in edges if f in valid_node_ids and t in valid_node_ids]
    
    # Only keep nodes that appear in edges
    used_node_ids = set()
    for f, t, _ in valid_edges:
        used_node_ids.add(f)
        used_node_ids.add(t)
    
    # Build remapping
    sorted_ids = sorted(used_node_ids)
    id_remap = {old_id: new_idx for new_idx, old_id in enumerate(sorted_ids)}
    
    # Remap nodes
    remapped_nodes = {}
    for old_id, new_idx in id_remap.items():
        remapped_nodes[new_idx] = nodes[old_id]
    
    # Remap edges (deduplicate)
    seen_edges = set()
    remapped_edges = []
    for f, t, rc in valid_edges:
        nf = id_remap[f]
        nt = id_remap[t]
        key = (min(nf, nt), max(nf, nt), rc)
        if key not in seen_edges:
            seen_edges.add(key)
            # Compute length in world pixels
            wx1, wy1 = remapped_nodes[nf]
            wx2, wy2 = remapped_nodes[nt]
            length = math.sqrt((wx2-wx1)**2 + (wy2-wy1)**2)
            remapped_edges.append((nf, nt, rc, length))
    
    return remapped_nodes, remapped_edges

def generate_cpp_header(nodes, edges, output_path):
    """Generate a C++ header file with the Melbourne map data."""
    print(f"Generating C++ header with {len(nodes)} nodes and {len(edges)} edges...")
    
    with open(output_path, 'w') as f:
        f.write("// AUTO-GENERATED — Melbourne CBD Road Network Data\n")
        f.write("// Source: OpenStreetMap (c) OpenStreetMap contributors, ODbL license\n")
        f.write("// Generated by tools/process_melbourne_osm.py\n")
        f.write("// DO NOT EDIT MANUALLY\n\n")
        f.write("#pragma once\n\n")
        f.write("#include <cstdint>\n\n")
        f.write("namespace acheron::world::melbourne_data\n{\n\n")
        
        # Constants
        f.write(f"    constexpr float MAP_WORLD_WIDTH  = {WORLD_WIDTH:.1f}f;\n")
        f.write(f"    constexpr float MAP_WORLD_HEIGHT = {WORLD_HEIGHT:.1f}f;\n")
        f.write(f"    constexpr float MAP_LAT_MIN = {LAT_MIN:.6f}f;\n")
        f.write(f"    constexpr float MAP_LAT_MAX = {LAT_MAX:.6f}f;\n")
        f.write(f"    constexpr float MAP_LON_MIN = {LON_MIN:.6f}f;\n")
        f.write(f"    constexpr float MAP_LON_MAX = {LON_MAX:.6f}f;\n\n")
        f.write(f"    constexpr uint32_t NODE_COUNT = {len(nodes)};\n")
        f.write(f"    constexpr uint32_t EDGE_COUNT = {len(edges)};\n\n")
        
        # Node struct
        f.write("    struct MapNode { float x; float y; };\n\n")
        
        # Edge struct (road_class: 0=motorway/trunk, 1=primary, 2=secondary, 3=tertiary, 4=residential, 5=service)
        f.write("    struct MapEdge { uint32_t from; uint32_t to; float length; uint8_t road_class; uint8_t _pad[3]; };\n\n")
        
        # Node data
        f.write(f"    constexpr MapNode NODES[{len(nodes)}] = {{\n")
        for i in range(len(nodes)):
            wx, wy = nodes[i]
            f.write(f"        {{ {wx:.2f}f, {wy:.2f}f }},\n")
        f.write("    };\n\n")
        
        # Edge data
        f.write(f"    constexpr MapEdge EDGES[{len(edges)}] = {{\n")
        for nf, nt, rc, length in edges:
            f.write(f"        {{ {nf}u, {nt}u, {length:.2f}f, {rc}u, {{0,0,0}} }},\n")
        f.write("    };\n\n")
        
        f.write("} // namespace acheron::world::melbourne_data\n")
    
    print(f"Written to {output_path}")

def main():
    import os
    
    # Try to load cached data first (the raw OSM file we already have)
    cache_file = "tools/melbourne_osm_raw.json"
    
    if os.path.exists(cache_file):
        print(f"Loading cached OSM data from {cache_file}")
        with open(cache_file, 'r', encoding='utf-8') as f:
            osm_data = json.load(f)
    else:
        osm_data = fetch_osm_data()
        if osm_data is None:
            print("Failed to fetch OSM data. Exiting.")
            sys.exit(1)
        # Cache it
        os.makedirs("tools", exist_ok=True)
        with open(cache_file, 'w', encoding='utf-8') as f:
            json.dump(osm_data, f)
        print(f"Cached to {cache_file}")
    
    print("Processing OSM data...")
    nodes, edges = process_osm_data(osm_data)
    print(f"Raw: {len(nodes)} nodes, {len(edges)} edges")
    
    nodes, edges = remap_ids(nodes, edges)
    print(f"Processed: {len(nodes)} nodes, {len(edges)} edges")
    
    output_path = "assets/data/melbourne_map_data.h"
    os.makedirs("assets/data", exist_ok=True)
    generate_cpp_header(nodes, edges, output_path)
    print("Done!")

if __name__ == "__main__":
    main()
