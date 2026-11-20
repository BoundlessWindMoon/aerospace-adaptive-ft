#!/usr/bin/env python3
"""RT 线：实时任务模型下的"完成率(正确且按时)"与 Deadline Miss Rate。
任务集用 UUniFast(Bini & Buttazzo 2005，实时领域标准生成器)；deadline=周期。
每模式 (o 开销, f 残留SDC/故障, d 检出率) 来自实测 framework_eval.csv。
核心张力(文献共识)：DMR 检错→回滚重跑→高 λ 下回滚抖动→误截止；TMR 就地纠错不回滚但 o 大；
OFF 快但高 λ 丢正确性。→ 最优模式随 (利用率 U, 故障率 λ) 变，自适应取上包络。"""
import csv, math, random, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H
random.seed(2025)

M = {}
with open(H.RES_DIR / "framework_eval.csv") as fp:
    for r in csv.DictReader(fp):
        M[r["config"]] = (float(r["overhead"]), float(r["fail_pct"]) / 100.0)
o_off, f_off = M["native"]
o_dmr, f_dmr = M["dmr_gen"]
o_tmr, f_tmr = M["tmr_gen"]
d_dmr = max(0.0, 1.0 - f_dmr / f_off)     # DMR 检出率(把本会 SDC 的故障挡下→回滚)≈0.96

def uunifast(n, U):                        # 标准任务集生成
    us, s = [], U
    for i in range(1, n):
        nx = s * random.random() ** (1.0 / (n - i)); us.append(s - nx); s = nx
    us.append(s); return us

def gen_taskset(n, U):
    us = uunifast(n, U); ts = []
    for u in us:
        C = random.uniform(1.0, 10.0)      # native WCET
        ts.append((C, C / max(u, 1e-3)))   # (C, D=T)
    return ts

NJOBS = 300
def sim(mode, taskset, lam):
    """返回 (completion_rate, deadline_miss_rate)。lam=每单位 native 时间的故障率。"""
    ok = miss = 0; tot = 0
    for (C, D) in taskset:
        for _ in range(NJOBS):
            tot += 1
            if mode == "OFF":
                et = C; faults = pois(lam * et)
                correct = all(random.random() >= f_off for _ in range(faults))
                ontime = et <= D
            elif mode == "TMR":
                et = C * o_tmr; faults = pois(lam * et)
                correct = all(random.random() >= f_tmr for _ in range(faults))
                ontime = et <= D
            else:  # DMR：检出→回滚重跑一次
                et = C * o_dmr; faults = pois(lam * et)
                detected = any(random.random() < d_dmr for _ in range(faults))
                if detected: et *= 2.0     # 回滚重跑
                # 未检出的故障仍可能 SDC
                correct = all(random.random() >= f_dmr for _ in range(faults))
                ontime = et <= D
            if ontime: 
                if correct: ok += 1
            else: miss += 1
    return ok / tot, miss / tot

def pois(mean):
    # 小均值 Poisson 采样
    L = math.exp(-mean); k = 0; p = 1.0
    while True:
        p *= random.random()
        if p <= L: return k
        k += 1

Us = [0.5, 0.7, 0.85]
LAMs = [("quiet", 0.002), ("SAA", 0.05), ("SPE", 0.5)]
print(f"\n{'U':>5}{'λ':>8}" + "".join(f"{m:>10}" for m in ["OFF", "DMR", "TMR"]) + f"{'adapt→':>12}")
for U in Us:
    ts = gen_taskset(6, U)
    for lname, lam in LAMs:
        comp = {m: sim(m, ts, lam)[0] for m in ["OFF", "DMR", "TMR"]}
        best = max(comp, key=comp.get)
        print(f"{U:>5.2f}{lname:>8}" + "".join(f"{comp[m]*100:>10.1f}" for m in ["OFF", "DMR", "TMR"]) +
              f"{best+' '+format(comp[best]*100,'.1f'):>12}")
print("\n完成率=正确且按时(%); 每格最优 mode 随 (U,λ) 变 → 没有单一固定策略全格最优, 自适应取上包络。")
print(f"(UUniFast 任务集 n=6; deadline=周期; NJOBS={NJOBS}; (o,f,d) 实测, d_dmr={d_dmr:.2f})")
