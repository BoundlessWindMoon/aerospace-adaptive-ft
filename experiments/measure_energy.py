#!/usr/bin/env python3
"""实测能量（Intel RAPL，非估算）：native / DMR / TMR 的 PKG+DRAM 焦耳。
方法(对齐 RAPL 文献 + CGO'25 容错论文)：perf 读 power/energy-pkg,energy-ram；
绑单核(socket0)跑；循环跑够秒级让能量>>噪声；减 idle 基线得可归因活跃能量；多次取中位。
输出每配置"每次全套件调用"的活跃焦耳与相对 native 的能量开销比。"""
import sys, re, subprocess, statistics, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

CORE = "2"          # socket0 上的一个核
K = 10              # 每次 perf 窗口内重复跑整套件的次数(单次≈0.5s，K=10→~5s 窗口)
REPEAT = 3          # 外层重复取中位
ISA = ["-atft-isa-avx512"]
APPROX = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier", "-atft-simd-f64"]
# 与 measure_framework.py 对齐的 7 配置，补齐"能量轴"。注意：共享节点被 co-tenant 污染，
# 绝对值不可信(idle~478W)，待干净 Intel 节点重跑；此处先把功能/管线跑通。
CFGS = [
    ("en_native",    "native",      ISA,                                         True),
    ("en_eddi",      "eddi",        ISA,                                         False),
    ("en_swift",     "swift",       ISA,                                         False),
    ("en_exact_tmr", "exact-tmr",   ISA,                                         False),
    ("en_inlane",    "approx-simd", APPROX + ISA,                                False),
    ("en_dmr",       "approx-simd", ["-atft-dmr-generic"],                       False),
    ("en_tmr",       "approx-simd", ["-atft-dmr-generic", "-atft-tmr-generic"],  False),
]

def perf_energy(argv):
    """跑 argv(list)，返回 (pkg_J, ram_J, elapsed_s)。"""
    cmd = ["perf", "stat", "-e", "power/energy-pkg/,power/energy-ram/",
           "--", "taskset", "-c", CORE] + argv
    r = subprocess.run(cmd, capture_output=True, text=True)
    out = r.stderr
    def g(pat):
        m = re.search(pat, out)
        return float(m.group(1).replace(",", "")) if m else float("nan")
    pkg = g(r"([\d.,]+)\s+Joules\s+power/energy-pkg/")
    ram = g(r"([\d.,]+)\s+Joules\s+power/energy-ram/")
    el  = g(r"([\d.,]+)\s+seconds time elapsed")
    return pkg, ram, el

# idle 基线功率(W)
ip, ir, it = [], [], []
for _ in range(3):
    pkg, ram, el = perf_energy(["sleep", "3"])
    ip.append(pkg/el); ir.append(ram/el)
P_idle_pkg = statistics.median(ip); P_idle_ram = statistics.median(ir)
print(f"idle: pkg={P_idle_pkg:.1f} W  ram={P_idle_ram:.1f} W")

SSA = H.build_infrastructure()
BIN = {}
for tag, mode, flags, gold in CFGS:
    b = Path(f"{H.BUILD_DIR}/bin_{tag}_x86_64")
    if not b.exists():            # 复用已编译的 bin；缺了才编
        H.compile_and_evaluate_policy(SSA, tag, mode, budget=1.0, fi_trials=0,
                                      run_fi=False, dump_golden=gold, pass_flags=flags)
    BIN[tag] = str(b)

def active_energy(binpath):
    """循环 K 次整套件，减 idle，返回 (活跃pkg_J, 活跃ram_J, elapsed) per-run 的中位。"""
    loop = f"for i in $(seq {K}); do {binpath} --pmu-perf x {H.GOLDEN_DIR} >/dev/null 2>&1; done"
    pk, rm = [], []
    for _ in range(REPEAT):
        pkg, ram, el = perf_energy(["sh", "-c", loop])
        pk.append((pkg - P_idle_pkg*el) / K)
        rm.append((ram - P_idle_ram*el) / K)
    return statistics.median(pk), statistics.median(rm)

print(f"\n{'config':12}{'pkg_mJ':>10}{'ram_mJ':>10}{'tot_mJ':>10}{'E/native':>10}", flush=True)
res = {}
for tag, *_ in CFGS:
    p, r = active_energy(BIN[tag])
    res[tag] = (p*1e3, r*1e3, (p+r)*1e3)   # J → mJ per suite-run
    print(f"{tag:12}{p*1e3:>10.3f}{r*1e3:>10.3f}{(p+r)*1e3:>10.3f}", flush=True)
base = res["en_native"][2]
print("\n相对 native 能量开销:", flush=True)
for tag, *_ in CFGS:
    print(f"  {tag:12} E/native = {res[tag][2]/base:.2f}", flush=True)
print("\n(活跃能量=总能量−idle功率×时长；PKG=处理器包，RAM=DRAM；绑 core", CORE, f"；K={K}×REPEAT={REPEAT} 中位)", flush=True)
