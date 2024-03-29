import os
import shutil
import sys

cwd = os.getcwd()
gem5_root = os.path.abspath("../../..")
gem5_bin = f"{gem5_root}/build/X86/gem5.opt"
config = f"{gem5_root}/configs/tdt4260/prefetcher.py"

num_benchmarks = 4

for x in range(num_benchmarks):
    os.chdir("spec2017")
    output_dir = f"prefetcher_out_{x}"
    if (os.path.exists(f"prefetcher_out_{x}")):
        shutil.rmtree(f"prefetcher_out_{x}")
    # run_ref = f"{gem5_bin} -r --debug-flags=TDTSimpleCache --outdir={output_dir} {config} --iteration {x}"
    run_ref = f"{gem5_bin} -r --outdir={output_dir} {config} --iteration {x}"
    os.system(run_ref)
    os.chdir(cwd)

result_dst = "results/results_summary.txt"

ipcs = []
others = []
for x in range(num_benchmarks):
    data = []
    with open(f"spec2017/prefetcher_out_{x}/stats.txt", "r") as stats:
        lines = stats.read().split("\n")
        for line in lines:
            line = line.split()
            if (len(line) == 0):
                continue
            if "ipc" in line[0]:
                ipcs.append(line[1])
            if "prefetcher" in line[0]:  # or "cache" in line[0]:
                data.append((line[0], line[1]))
    others.append(data)

b_names = ["gcc_s", "exchange2_s", "mcf_s", "deepsjeng_s"]

with open(result_dst, "w") as out:
    out.write("Summary of benchmarking follows...\n")
    out.write("-----IPC-----\n")
    for x in range(len(ipcs)):
        out.write(f"{b_names[x]}: {ipcs[x]}\n")

    out.write("\n\n\n")

    out.write("Detailed stats follow...\n")
    for x in range(len(others)):
        out.write(f"{b_names[x]} stats:\n")
        for y in others[x]:
            out.write(f"{y[0]}: {y[1]}\n")
        out.write("\n\n\n")
