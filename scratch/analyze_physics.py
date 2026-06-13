import os
import glob
import re

def analyze_physics(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    data = []
    for line in lines:
        if not line.startswith('#'):
            parts = line.strip().split(',')
            if len(parts) >= 4 and parts[0] != 'Time (s)':
                try:
                    data.append({
                        'time': float(parts[0]),
                        'depth': float(parts[1]),
                        'actuator': int(parts[2]),
                        'target': int(parts[3])
                    })
                except ValueError:
                    pass
                    
    if len(data) < 10:
        return
        
    print(f"=== Physics Analysis: {os.path.basename(filepath)} ===")
    
    # Calculate velocities
    velocities = []
    for i in range(1, len(data)):
        dt = data[i]['time'] - data[i-1]['time']
        dd = data[i]['depth'] - data[i-1]['depth']
        if dt > 0:
            velocities.append(dd / dt)
            
    max_sink = max(velocities) if velocities else 0
    max_rise = min(velocities) if velocities else 0
    
    print(f"  Max Descent Speed: {max_sink:.3f} m/s")
    print(f"  Max Ascent Speed:  {abs(max_rise):.3f} m/s")
    
    # Analyze actuator response time
    # Find when target changes and track how long it takes for the actuator to catch up
    lag_times = []
    for i in range(1, len(data)):
        if abs(data[i]['target'] - data[i-1]['target']) > 200:
            # Target changed significantly, let's see how long to reach it
            target = data[i]['target']
            start_time = data[i]['time']
            reached = False
            for j in range(i, len(data)):
                # If actuator is within 100 units of target
                if abs(data[j]['actuator'] - target) <= 100:
                    lag_times.append(data[j]['time'] - start_time)
                    reached = True
                    break
            if not reached:
                # Did not reach within log window
                pass
                
    if lag_times:
        avg_act_lag = sum(lag_times) / len(lag_times)
        print(f"  Average Actuator Transit Lag: {avg_act_lag:.2f} seconds")
    else:
        print("  Average Actuator Transit Lag: Could not determine (target changes too small or slow)")
        
    # Analyze buoyancy response delay
    # Find when the actuator crosses the neutral point, and see how long before the float reverses direction
    # Let's assume neutral is around 1600.
    reversals = 0
    for i in range(2, len(velocities)):
        # Reversal of direction (vel changes sign)
        if (velocities[i-1] > 0.02 and velocities[i] < -0.02) or (velocities[i-1] < -0.02 and velocities[i] > 0.02):
            reversals += 1
            # Let's look back and see when the actuator crossed 1600
            cross_idx = None
            for k in range(i, 0, -1):
                act_k = data[k]['actuator']
                act_prev = data[k-1]['actuator']
                if (act_k >= 1600 and act_prev < 1600) or (act_k <= 1600 and act_prev > 1600):
                    cross_idx = k
                    break
            if cross_idx is not None:
                delay = data[i]['time'] - data[cross_idx]['time']
                print(f"  Reversal #{reversals} at depth {data[i]['depth']:.2f}m. Time since actuator crossed neutral: {delay:.2f} seconds")
                
    print("-" * 60)

def main():
    profile_dir = r"C:\Users\aman\Documents\Programming\X18-Float-Embedded\front_end\profiles"
    files = sorted(glob.glob(os.path.join(profile_dir, "profile_20260502_*.csv")))
    
    # Analyze a few key files representing both yoyo and other behavior
    analyze_physics(files[0]) # 134841
    analyze_physics(files[5]) # 152814 (yoyo)
    analyze_physics(files[9]) # 161336 (yoyo)
    analyze_physics(files[12]) # 164310 (yoyo)
    analyze_physics(files[14]) # 170349 (last run)

if __name__ == '__main__':
    main()
