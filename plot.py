import numpy as np
import matplotlib.pyplot as plt
import os

fresults = {}
for i, fname in enumerate(os.listdir("results/")):
    with open(f"results/{fname}") as f:
        lines = f.readlines()
    num_results = len(lines)//3
    results = np.empty((num_results, 7))
    for i in range(num_results):
        config = lines[i*3].split(", ")
        ipc = float(lines[i*3 + 1])
        results[i] = np.array(config + [ipc])

    sorted_idx = np.argsort(results[:, 6])
    sorted_results = np.empty_like(results)

    for i, idx in enumerate(sorted_idx):
        sorted_results[i] = results[idx]

    sorted_results = np.flip(sorted_results, axis=0)
    fresults[fname] = sorted_results

plt.figure(figsize=(15, 10))
param = ["ALUs", "MULs", "LSUs", "Stations per FU", "Pregs", "Fetch Width"]
for i in range(6):
    plt.subplot(3, 2, i+1)
    plt.xlabel(param[i])
    plt.ylabel("Average IPC")
    for j, (fname, results) in enumerate(fresults.items()):
        x = []
        y = []
        for val in np.unique(results[:, i]):
            x.append(val)
            y.append(np.mean(results[results[:, i] == val][:, 6]))
        plt.plot(x, y, label=fname)
    plt.legend()

plt.tight_layout()
plt.show()

    # ipc_thresh = 0.9 * np.max(results[:, 6])
    # top_results = results[results[:, 6] >= ipc_thresh]
    # least_fus = np.argmin(top_results[:, 0] + top_results[:, 1] + top_results[:, 2])

