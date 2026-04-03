import cma
import subprocess
import sys
import threading

EXECUTABLE  = "./score.exe" 
TIMEOUT_SEC = 600
_print_lock = threading.Lock()

def run_game(x, rollouts, iterations) -> int:
    try:
        proc = subprocess.run(
            [EXECUTABLE, str(x[0]), str(x[1]), str(x[2]), str(rollouts), str(iterations)],
            capture_output=True, text=True,
            timeout=TIMEOUT_SEC,
        )

        return int(proc.stdout.strip())
    except subprocess.TimeoutExpired:
        with _print_lock:
            print(f"TIMEOUT after {TIMEOUT_SEC}s")
        return None
    except FileNotFoundError:
        with _print_lock:
            print(f"\nERROR: executable '{EXECUTABLE}' not found.")
        sys.exit(1)

num_rollouts   = 250 # for sake of practicality
num_iterations = 10  # also for sake of my laptop gg

def eval(x):
    # negate for fmin2
    return -run_game(x, num_rollouts, num_iterations)

options = cma.CMAOptions()
options['bounds'] = [
    [0.01, 0.1, 0.1],   # lower bounds for [C, open, mono]
    [2.0,  1.0, 1.0],   # upper bounds
]
x, es = cma.fmin2(eval, [0.05, 0.5, 0.5], 0.02, options)
print(x)