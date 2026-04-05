import cma
import subprocess
import json
import csv
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed

EXECUTABLE  = "./score.exe"
TIMEOUT_SEC = 600
NUM_WORKERS = 12
LOG_FILE    = "cma_history.csv"
RESULT_FILE = "cma_best_result.json"

def run_game(x, rollouts, iterations) -> int:
    try:
        proc = subprocess.run(
            [EXECUTABLE, str(x[0]), str(x[1]), str(x[2]), str(x[3]),
             str(rollouts), str(iterations)],
            capture_output=True, text=True,
            timeout=TIMEOUT_SEC,
        )
        return int(proc.stdout.strip())
    except subprocess.TimeoutExpired:
        print(f"TIMEOUT after {TIMEOUT_SEC}s")
        return None

num_rollouts   = 500
num_iterations = 20

options = cma.CMAOptions()
options['bounds'] = [
    [0.01, 0.01, 0.01, 0.01],
    [1.0,  1.0,  1.0,  1.0],
]
options['scaling_of_variables'] = [0.2, 0.1, 2.0, 1.5]

es = cma.CMAEvolutionStrategy([0.0937, 0.0559, 0.8575, 0.4177], 0.2, options)

# Open CSV log once, write header
with open(LOG_FILE, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["generation", "fbest", "x0", "x1", "x2", "x3"])

generation = 0

with ThreadPoolExecutor(max_workers=NUM_WORKERS) as pool:
    while not es.stop():
        candidates = es.ask()

        futures = {pool.submit(run_game, x, num_rollouts, num_iterations): i
                   for i, x in enumerate(candidates)}
        
        fitnesses = [None] * len(candidates)
        for future in as_completed(futures):
            idx = futures[future]
            result = future.result()
            fitnesses[idx] = -result if result is not None else 0

        es.tell(candidates, fitnesses)
        es.disp()
        generation += 1

        # Save xbest after every generation
        xbest  = es.result.xbest
        fbest  = es.result.fbest  # negated, so negate back for readability

        with open(LOG_FILE, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([generation, -fbest, *xbest])

# Final save — full result object as JSON
result = es.result
final = {
    "timestamp": datetime.now().isoformat(),
    "xbest":     list(result.xbest),
    "fbest":     -result.fbest,          # un-negate to get original score
    "evals":     result.evaluations,
    "generations": generation,
}
with open(RESULT_FILE, "w") as f:
    json.dump(final, f, indent=2)

print(f"\nBest parameters: {result.xbest}")
print(f"Best score:      {-result.fbest}")
print(f"Saved to {RESULT_FILE} and {LOG_FILE}")