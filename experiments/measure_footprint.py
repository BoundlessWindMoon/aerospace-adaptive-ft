#!/usr/bin/env python3
"""B1: 代码体积 + 数据段/栈 footprint 表（星载码空间/SRAM 硬约束）。

对每个保护配置，把变换后的"被测程序 IR"(不含 eval_suite harness) 单独 llc 成目标文件，
用 `size -A` 量 .text/.rodata/.data/.bss；再用 llc -stack-size-section 取每函数栈深的最大值。
一行一个 config。产出 results/footprint.csv。
P0-6 防护：若某 config 的 .text 相对 native 无变化，标 NOOP(可能机制静默未插)。
"""
import csv
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
OPT = "opt-15"
LLC = "llc-15"
PASS = BUILD / "libAdaptiveFTPass.so"
SSA = BUILD / "benchmarks_ssa.ll"

STRUCT = ["-atft-dmr-generic", "-atft-addr-dmr", "-atft-int-residue", "-atft-cmp-dmr",
          "-atft-abft-insert", "-atft-layer-exclusive", "-atft-no-cse-barrier",
          "-atft-simd-f64", "-atft-replica-exact"]
# (name, mode, extra_flags)
CONFIGS = [
    ("native",         "native",      []),
    ("exact_tmr",      "exact-tmr",   []),
    ("dmr_generic",    "approx-simd", ["-atft-dmr-generic", "-atft-simd-f64", "-atft-replica-exact"]),
    ("struct_full",    "approx-simd", STRUCT),
]
COMMON = ["-atft-rank=semantic", "-atft-budget=1.0", "-atft-delta=5e-5",
          "-atft-w1=2.5", "-atft-w2=1.8", "-atft-w3=0.8"]


def run(cmd):
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)


def build_obj(name, mode, extra):
    sut = BUILD / f"fp_{name}.ll"
    obj = BUILD / f"fp_{name}.o"
    r = run([OPT, f"-load-pass-plugin={PASS}", "-passes=adaptive-ft,verify",
             f"-atft-mode={mode}", *extra, *COMMON, "-S", str(SSA), "-o", str(sut)])
    if r.returncode:
        print(r.stderr[-1500:]); sys.exit(f"opt fail {name}")
    r = run([LLC, "-O3", "-stack-size-section", "-filetype=obj", str(sut), "-o", str(obj)])
    if r.returncode:
        print(r.stderr[-1500:]); sys.exit(f"llc fail {name}")
    return obj


def sizes(obj):
    """返回 .text/.rodata/.data/.bss 字节 (size -A 口径)。"""
    out = run(["size", "-A", str(obj)]).stdout
    sec = {".text": 0, ".rodata": 0, ".data": 0, ".bss": 0}
    for line in out.splitlines():
        m = re.match(r"\s*(\.\S+)\s+(\d+)", line)
        if m and m.group(1) in sec:
            sec[m.group(1)] = int(m.group(2))
    return sec


def max_stack(obj):
    """从 .stack_sizes 节取最大函数栈深 (字节)。llc -stack-size-section 产出。"""
    out = run(["llvm-readobj-15", "--stack-sizes", str(obj)]).stdout
    mx = 0
    for m in re.finditer(r"Size:\s*(\d+)", out):
        mx = max(mx, int(m.group(1)))
    return mx


def main():
    rows = []
    nat = None
    for name, mode, extra in CONFIGS:
        obj = build_obj(name, mode, extra)
        s = sizes(obj)
        s["stack_max"] = max_stack(obj)
        s["code"] = s[".text"] + s[".rodata"]      # 码空间(flash)
        s["sram"] = s[".data"] + s[".bss"]          # 静态 SRAM
        rows.append((name, s))
        if name == "native":
            nat = s
    print(f"{'config':14}{'.text':>9}{'.rodata':>9}{'.data':>8}{'.bss':>8}{'stack':>8}"
          f"{'code×':>8}{'状态':>8}")
    out_rows = []
    for name, s in rows:
        code_ratio = s["code"] / nat["code"] if nat["code"] else 0.0
        status = "NOOP" if (name != "native" and s[".text"] == nat[".text"]) else "ok"
        print(f"{name:14}{s['.text']:>9}{s['.rodata']:>9}{s['.data']:>8}{s['.bss']:>8}"
              f"{s['stack_max']:>8}{code_ratio:>7.2f}x{status:>8}")
        out_rows.append({"config": name, "text": s[".text"], "rodata": s[".rodata"],
                         "data": s[".data"], "bss": s[".bss"], "stack_max": s["stack_max"],
                         "code_bytes": s["code"], "sram_bytes": s["sram"],
                         "code_ratio_vs_native": round(code_ratio, 3), "status": status})
    outp = ROOT / "results" / "footprint.csv"
    with open(outp, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(out_rows[0].keys()))
        w.writeheader(); w.writerows(out_rows)
    print(f"\nCSV -> {outp}")
    print("口径: 仅被测程序 IR(不含 harness), llc -O3; code=.text+.rodata(flash), sram=.data+.bss")


if __name__ == "__main__":
    main()
