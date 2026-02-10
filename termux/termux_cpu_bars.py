import os
import time
import sys

USER_HZ = 100
BAR_WIDTH = 30
SMOOTHING_FACTOR = 0.15 
UPDATE_INTERVAL = 1.0

def get_cpu_freq(core):
    try:
        with open(f'/sys/devices/system/cpu/cpu{core}/cpufreq/scaling_cur_freq', 'r') as f:
            freq_khz = int(f.read().strip())
            return freq_khz / 1_000_000.0
    except:
        return 0.0

def main():
    try:
        with open('/sys/devices/system/cpu/online', 'r') as f:
            line = f.read().strip()
            if '-' in line:
                s, e = map(int, line.split('-'))
                cores = list(range(s, e + 1))
            else:
                cores = [int(line)]
    except:
        cores = list(range(8))

    last_stats = {} 
    last_time = time.time()
    smoothed_loads = {c: 0.0 for c in cores}
    
    HIDE_CURSOR = "\033[?25l"
    SHOW_CURSOR = "\033[?25h"
    HOME = "\033[H"
    COLOR_GREEN = "\033[32m"
    COLOR_YELLOW = "\033[33m"
    COLOR_RED = "\033[31m"
    COLOR_RESET = "\033[0m"

    sys.stdout.write(HIDE_CURSOR + "\033[2J")

    try:
        while True:
            iter_start = time.time()
            dt = iter_start - last_time
            hz_dt = dt * USER_HZ
            
            curr_stats = {}
            core_ticks_diff = [0] * 32 
            
            try:
                with os.scandir('/proc') as it:
                    for entry in it:
                        if entry.name.isdigit():
                            task_path = f"/proc/{entry.name}/task"
                            try:
                                with os.scandir(task_path) as tit:
                                    for tentry in tit:
                                        try:
                                            with open(f"{task_path}/{tentry.name}/stat", 'r') as f:
                                                data = f.read().rsplit(')', 1)[1].split()
                                                t = int(data[11]) + int(data[12])
                                                c = int(data[36])
                                                tid_key = (entry.name, tentry.name)
                                                curr_stats[tid_key] = t
                                                if tid_key in last_stats:
                                                    diff = t - last_stats[tid_key]
                                                    if diff > 0 and c < 32:
                                                        core_ticks_diff[c] += diff
                                        except: continue
                            except: continue
            except: pass

            out = [HOME]
            out.append(f"yeah CPU Monitor - {time.strftime('%H:%M:%S')}\n")
            out.append("=" * 60 + "\n")
            
            for c in cores:
                raw_load = (core_ticks_diff[c] / hz_dt) * 100
                raw_load = max(0.0, min(100.0, raw_load))
                
                smoothed_loads[c] = (SMOOTHING_FACTOR * raw_load) + ((1 - SMOOTHING_FACTOR) * smoothed_loads[c])
                load = smoothed_loads[c]
                freq = get_cpu_freq(c)
                
                if load <= 50: color = COLOR_GREEN
                elif load <= 80: color = COLOR_YELLOW
                else: color = COLOR_RED
                
                filled = int(BAR_WIDTH * load / 100)
                bar = f"{color}{'█' * filled}{'░' * (BAR_WIDTH - filled)}{COLOR_RESET}"
                out.append(f"Core {c:<2} [{bar}] {load:>6.1f}%  @ {freq:>4.2f} GHz\n")
            
            out.append("=" * 60 + "\n")
            
            sys.stdout.write("".join(out))
            sys.stdout.flush()
            
            last_stats = curr_stats
            last_time = iter_start
            
            time.sleep(UPDATE_INTERVAL)

    except KeyboardInterrupt:
        sys.stdout.write(SHOW_CURSOR + "\nBeendet.\n")

if __name__ == "__main__":
    main()
