import os
import glob
import re

def analyze_profile(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    metadata = {}
    data = []
    
    for line in lines:
        if line.startswith('#'):
            # Parse metadata
            match = re.match(r'#\s*([^:]+):\s*(.*)', line)
            if match:
                key, val = match.groups()
                metadata[key.strip()] = val.strip()
        else:
            # Parse CSV data
            parts = line.strip().split(',')
            if len(parts) >= 3 and parts[0] != 'Time (s)':
                try:
                    data.append({
                        'time': float(parts[0]),
                        'depth': float(parts[1]),
                        'actuator': int(parts[2]),
                        'target': int(parts[3])
                    })
                except ValueError:
                    pass
                    
    if not data:
        return None
        
    depths = [d['depth'] for d in data]
    max_depth = max(depths)
    min_depth = min(depths)
    
    # Extract target depth
    deep_target = float(metadata.get('Deep Target', '0').replace(' m', ''))
    shallow_target = float(metadata.get('Shallow Target', '0').replace(' m', ''))
    pid = metadata.get('PID', 'Unknown')
    neutral = metadata.get('Neutral ADC', 'Unknown')
    bounds = metadata.get('Bounds', 'Unknown')
    
    print(f"File: {os.path.basename(filepath)}")
    print(f"  PID: {pid} | Neutral ADC: {neutral} | Bounds: {bounds}")
    print(f"  Targets: Deep={deep_target}m, Shallow={shallow_target}m")
    print(f"  Depth Range: {min_depth:.2f}m to {max_depth:.2f}m")
    
    # Simple check for yoyo: count how many times depth crosses the deep target after first reaching it
    crossings = 0
    crossed_first = False
    for i in range(1, len(depths)):
        # Check if crossed deep_target
        if not crossed_first:
            if depths[i] >= deep_target:
                crossed_first = True
        else:
            # Check if it crossed back and forth
            if (depths[i-1] < deep_target and depths[i] >= deep_target) or (depths[i-1] > deep_target and depths[i] <= deep_target):
                crossings += 1
                
    print(f"  Target crossings after arrival: {crossings}")
    print("-" * 50)

def main():
    profile_dir = r"C:\Users\aman\Documents\Programming\X18-Float-Embedded\front_end\profiles"
    files = sorted(glob.glob(os.path.join(profile_dir, "profile_20260502_*.csv")))
    
    for f in files:
        analyze_profile(f)

if __name__ == '__main__':
    main()
