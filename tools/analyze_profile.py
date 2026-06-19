#!/usr/bin/env python3
import os
import sys
import glob
import pandas as pd
import numpy as np

def analyze_log(filename):
    if not os.path.exists(filename):
        print(f"Error: File '{filename}' does not exist.")
        return

    print(f"\n==========================================")
    print(f"Analyzing {os.path.basename(filename)}")
    print(f"==========================================")
    
    # Read headers to get settings
    headers = {}
    with open(filename, 'r') as f:
        for line in f:
            if line.startswith("#"):
                parts = line.strip('# \n').split(': ')
                if len(parts) == 2:
                    headers[parts[0]] = parts[1]
            else:
                break
                
    print(f"PID Parameters: {headers.get('PID', 'N/A')}")
    print(f"Neutral ADC: {headers.get('Neutral ADC', 'N/A')}")
    print(f"Deep/Shallow Targets: {headers.get('Deep Target', 'N/A')} / {headers.get('Shallow Target', 'N/A')}")
    
    # Read data
    try:
        df = pd.read_csv(filename, comment='#')
    except Exception as e:
        print(f"Error reading CSV: {e}")
        return
    
    if df.empty:
        print("Empty log file.")
        return
        
    print(f"Data points: {len(df)}")
    print(f"Time Range: {df['Time (s)'].min():.1f}s to {df['Time (s)'].max():.1f}s")
    print(f"Depth Range: {df['Depth (m)'].min():.3f}m to {df['Depth (m)'].max():.3f}m")
    
    # Analyze Deep Target Holding (Target = 2.50m)
    deep_holding = df[(df['Depth (m)'] > 2.0) & (df['State'] == 2)]
    if not deep_holding.empty:
        mean_depth = deep_holding['Depth (m)'].mean()
        std_depth = deep_holding['Depth (m)'].std()
        max_depth = deep_holding['Depth (m)'].max()
        error = mean_depth - 2.50
        print("\n--- Deep Stage Hold (Target: 2.50m) ---")
        print(f"  Average Depth: {mean_depth:.3f} m (Error: {error:+.3f} m)")
        print(f"  Depth Standard Dev (Oscillation): {std_depth:.4f} m")
        print(f"  Maximum Overshoot Depth: {max_depth:.3f} m")
        if 'Actuator (ADC)' in df.columns:
            print(f"  Actuator Range (ADC): {deep_holding['Actuator (ADC)'].min()} to {deep_holding['Actuator (ADC)'].max()}")
            print(f"  Average Actuator Pos: {deep_holding['Actuator (ADC)'].mean():.1f} ADC")
        
    # Analyze Shallow Stage Holding (Target = 0.40m)
    shallow_holding = df[(df['Depth (m)'] < 1.0) & (df['Depth (m)'] > 0.1) & (df['State'] == 2) & (df['Time (s)'] > 40)]
    if not shallow_holding.empty:
        mean_depth = shallow_holding['Depth (m)'].mean()
        std_depth = shallow_holding['Depth (m)'].std()
        min_depth = shallow_holding['Depth (m)'].min()
        error = mean_depth - 0.40
        print("\n--- Shallow Stage Hold (Target: 0.40m) ---")
        print(f"  Average Depth: {mean_depth:.3f} m (Error: {error:+.3f} m)")
        print(f"  Depth Standard Dev (Oscillation): {std_depth:.4f} m")
        print(f"  Maximum Overshoot Depth (Min): {min_depth:.3f} m")
        if 'Actuator (ADC)' in df.columns:
            print(f"  Actuator Range (ADC): {shallow_holding['Actuator (ADC)'].min()} to {shallow_holding['Actuator (ADC)'].max()}")
            print(f"  Average Actuator Pos: {shallow_holding['Actuator (ADC)'].mean():.1f} ADC")

def main():
    if len(sys.argv) > 1:
        # Use provided files
        files = sys.argv[1:]
    else:
        # Default to finding the last 3 CSV logs in front_end/profiles/
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.dirname(script_dir)
        profiles_dir = os.path.join(project_root, "front_end", "profiles")
        
        if not os.path.exists(profiles_dir):
            print(f"Error: Profiles directory '{profiles_dir}' not found.")
            sys.exit(1)
            
        csv_files = glob.glob(os.path.join(profiles_dir, "profile_*.csv"))
        if not csv_files:
            print(f"No profiles found in '{profiles_dir}'.")
            sys.exit(0)
            
        # Sort by filename (which includes timestamp) and grab the last 3
        csv_files.sort()
        files = csv_files[-3:]
        print(f"No files specified. Analyzing the 3 most recent profile logs:")
        for f in files:
            print(f"  - {os.path.basename(f)}")

    for filename in files:
        analyze_log(filename)

if __name__ == "__main__":
    main()
