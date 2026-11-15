#!/usr/bin/env python3
"""每内核静态向量化度量（x86 -O3 -march=native）。

把 benchmarks.c 编到 .s，或反汇编 harness 实际构建的 native SUT 二进制，按
bench_pb_* 函数区间统计浮点算术指令的标量/打包构成与 lane 加权平均向量宽度。
用来作为"原生计算用了多少 SIMD"的代理量 —— 这是开销-向量化关系（指令级冗余抢
lane）那条曲线的横轴。纯静态、不碰保护 pass。

权威口径用 objdump 真二进制（反映两段式 opt→-O3 的真实向量化）：
  python3 experiments/measure_vectorization.py <native_sut_binary>
缺省（无参数）走 benchmarks.c 单次 -O3 代理口径。

输出 results/opt/kernel_vectorization.csv:
  benchmark, total_fp, scalar_fp, packed_fp, packed_frac, mean_vec_width, max_vec_width
"""

import csv
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "results" / "opt" / "kernel_vectorization.csv"
CLANG = "clang-15"
INCS = [
    "include", "benchmarks/obpmark_shim", "third_party/polybench-c/utilities",
    "third_party/kissfft", "third_party/obpmark/src/common",
    "third_party/obpmark/src/1.1-image/src/ports/cpu",
]

# 浮点算术指令（排除纯 mov/load/store/shuffle/convert——那些不代表计算宽度）
# 用 search：.s 行是 "\tvmulpd ..."，objdump 行是 "  4xx:\tvmulpd ..."，都能命中。
FP_OP = re.compile(
    r"\bv?(mul|add|sub|div|fmadd\d*|fnmadd\d*|fmsub\d*|fnmsub\d*|max|min|sqrt)(sd|ss|pd|ps)\b"
)
REG_BYTES = {"xmm": 16, "ymm": 32, "zmm": 64}
DTYPE_BYTES = {"d": 8, "s": 4}


def gen_asm(tmp_s):
    cmd = [CLANG, "-O3", "-march=native", "-S", "benchmarks/benchmarks.c"]
    for d in INCS:
        cmd.append(f"-I{d}")
    cmd += ["-o", str(tmp_s)]
    subprocess.run(cmd, cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def op_lanes(line, suffix):
    """这条浮点指令覆盖多少个元素 lane。scalar=1；packed 按最宽寄存器算。"""
    if suffix in ("sd", "ss"):
        return 1
    dt = DTYPE_BYTES[suffix[1]]            # pd->d, ps->s
    width = 16                             # 默认 xmm
    for reg, b in REG_BYTES.items():
        if "%" + reg in line:
            width = max(width, b)
    return width // dt


def parse(lines, label_fn):
    """按顶层符号标签切函数，统计每个 bench_pb_* 区间的浮点构成。
    label_fn(line) -> 符号名 或 None。"""
    rows = []
    cur = None
    agg = {}

    def flush():
        if cur is None:
            return
        total = agg["scalar"] + agg["packed"]
        if total == 0:
            return
        name = cur[len("bench_"):]         # bench_pb_gemm -> pb_gemm
        rows.append({
            "benchmark": name,
            "total_fp": total,
            "scalar_fp": agg["scalar"],
            "packed_fp": agg["packed"],
            "packed_frac": round(agg["packed"] / total, 4),
            "mean_vec_width": round(agg["lanes"] / total, 3),
            "max_vec_width": agg["maxw"],
        })

    for line in lines:
        sym = label_fn(line)
        if sym is not None:                 # 函数边界
            flush()
            if sym.startswith("bench_pb_"):
                cur = sym
                agg = {"scalar": 0, "packed": 0, "lanes": 0, "maxw": 1}
            else:
                cur = None
            continue
        if cur is None:
            continue
        fm = FP_OP.search(line)
        if not fm:
            continue
        suffix = fm.group(2)
        lanes = op_lanes(line, suffix)
        if suffix in ("sd", "ss"):
            agg["scalar"] += 1
        else:
            agg["packed"] += 1
        agg["lanes"] += lanes
        agg["maxw"] = max(agg["maxw"], lanes)
    flush()
    return rows


_ASM_LABEL = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*):")
_OBJ_LABEL = re.compile(r"^[0-9a-f]+ <([A-Za-z_][A-Za-z0-9_.]*)>:")


def from_asm():
    tmp_s = ROOT / "build" / "benchmarks_native.s"
    tmp_s.parent.mkdir(parents=True, exist_ok=True)
    gen_asm(tmp_s)
    with open(tmp_s) as f:
        return parse(f.readlines(), lambda ln: (_ASM_LABEL.match(ln).group(1)
                                                if _ASM_LABEL.match(ln) else None))


def from_binary(path):
    out = subprocess.run(["objdump", "-d", "--no-show-raw-insn", str(path)],
                         check=True, stdout=subprocess.PIPE, text=True).stdout
    return parse(out.splitlines(), lambda ln: (_OBJ_LABEL.match(ln).group(1)
                                               if _OBJ_LABEL.match(ln) else None))


def main():
    if len(sys.argv) > 1:
        rows = from_binary(sys.argv[1])
        src = f"objdump {sys.argv[1]}"
    else:
        rows = from_asm()
        src = "benchmarks.c -O3 (proxy)"
    rows.sort(key=lambda r: -r["mean_vec_width"])
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"-> {OUT}  ({len(rows)} kernels, src={src})")
    print(f"{'kernel':24s}{'fp':>6}{'scal':>6}{'pack':>6}{'pack%':>7}{'meanW':>7}{'maxW':>6}")
    for r in rows:
        print(f"{r['benchmark']:24s}{r['total_fp']:6d}{r['scalar_fp']:6d}"
              f"{r['packed_fp']:6d}{r['packed_frac']*100:6.1f}%{r['mean_vec_width']:7.2f}"
              f"{r['max_vec_width']:6d}")


if __name__ == "__main__":
    sys.exit(main())
