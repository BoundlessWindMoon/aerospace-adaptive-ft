#!/usr/bin/env python3
"""非侵入式 gap 分解：PMU(insts/cycles) 把 FP 三模税拆成 '多指令' vs 'IPC(spill/stall)'，
再 objdump 数栈访问占比当 spill 代理。全部只读产物/硬件计数，不改被测计算。"""
import math, sys, csv, re, subprocess
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier"]
TRIALS = 0  # 只要 PMU + 二进制，不跑 FI
CFGS = [
    ("native", "native",        []),
    ("scalar", "approx-scalar",  BASE),
    ("simd32", "approx-simd",    BASE),
    ("simd64", "approx-simd",    BASE + ["-atft-simd-f64"]),
]

def stack_frac(bin_path):
    """objdump：栈相对访存指令(spill/reload 代理) 占全部指令的比例。"""
    try:
        out = subprocess.run(["objdump", "-d", str(bin_path)], capture_output=True,
                             text=True, timeout=120).stdout
    except Exception:
        return float("nan"), 0
    insn = re.findall(r"^\s+[0-9a-f]+:\t[0-9a-f ]+\t(.*)$", out, re.M)
    if not insn:
        return float("nan"), 0
    stack = sum(1 for i in insn if re.search(r"\(%r[sb]p\)", i) and re.match(r"\s*(mov|movs|movap|movup|movdq)", i))
    return stack / len(insn), len(insn)

SSA = H.build_infrastructure()
T, I, C, SF = {}, {}, {}, {}
for tag, mode, flags in CFGS:
    T[tag], I[tag], C[tag] = {}, {}, {}
    p, _ = H.compile_and_evaluate_policy(SSA, f"gap_{tag}", mode, budget=1.0,
                                         fi_trials=0, run_fi=False, pass_flags=flags)
    for r in p:
        T[tag][r["benchmark"]] = r["exec_time_us"]
        I[tag][r["benchmark"]] = r["hw_insts"]
        C[tag][r["benchmark"]] = r["hw_cycles"]
    SF[tag] = stack_frac(H.BUILD_DIR / f"bin_gap_{tag}_x86_64")

def geo(xs):
    xs = [x for x in xs if x > 0]
    return math.exp(sum(map(math.log, xs))/len(xs)) if xs else float("nan")

benches = list(T["native"].keys())
print(f"\n{'config':9} {'time_tax':>9} {'inst_tax':>9} {'cyc_tax':>8} {'IPC比':>7} {'stack%':>7}")
rows = []
for tag, *_ in CFGS:
    if tag == "native":
        print(f"{tag:9} {'1.00':>9} {'1.00':>9} {'1.00':>8} {'1.00':>7} {SF[tag][0]*100:>6.1f}")
        continue
    tt = geo([T[tag][b]/T['native'][b] for b in benches if b in T[tag]])
    it = geo([I[tag][b]/I['native'][b] for b in benches if b in I[tag]])
    ct = geo([C[tag][b]/C['native'][b] for b in benches if b in C[tag]])
    ipc = it/ct  # IPC_prot/IPC_nat = (I_p/C_p)/(I_n/C_n) = inst_tax/cyc_tax
    print(f"{tag:9} {tt:>9.2f} {it:>9.2f} {ct:>8.2f} {ipc:>7.2f} {SF[tag][0]*100:>6.1f}")
    rows.append({"config": tag, "time_tax": round(tt,3), "inst_tax": round(it,3),
                 "cyc_tax": round(ct,3), "ipc_ratio": round(ipc,3), "stack_frac": round(SF[tag][0],4)})

csv_path = Path(H.ROOT_DIR) / "results" / "opt" / "gap_nonintrusive.csv"
with open(csv_path, "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=["config","time_tax","inst_tax","cyc_tax","ipc_ratio","stack_frac"])
    w.writeheader(); w.writerows(rows)
print(f"\nIPC比<1 = 冗余跑得比原程序慢(spill/stall)；≈1 = 税纯是多指令。wrote {csv_path}")
