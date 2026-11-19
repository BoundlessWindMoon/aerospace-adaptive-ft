#!/usr/bin/env python3
"""管道税 vs 校验税 vs 冗余 的 IR 分解(非侵入,只读 opt 产物)：
判断 4.48× 之上还剩什么——若仍是"老式投票(fabs/fsub/fcmp/select)"占大头→把 PHI/边界投票
也转签名有头;若管道(extract/insert)大→全程向量化有头;若只剩 xor+冗余→已近 2× 地板。"""
import sys, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier",
        "-atft-simd-f64", "-atft-loop-vote"]

def opcount(ll):
    t = Path(ll).read_text()
    insts = re.findall(r"^\s+(?:%\S+\s*=\s*)?([a-z][a-z0-9.]*)\s", t, re.M)
    from collections import Counter
    c = Counter(insts)
    total = sum(1 for _ in re.findall(r"^\s+[^;]", t, re.M))
    return c, len(insts)

SSA = H.build_infrastructure()
H.compile_and_evaluate_policy(SSA, "plumb_native", "native", budget=1.0, fi_trials=0, run_fi=False)
H.compile_and_evaluate_policy(SSA, "plumb_prot", "approx-simd", budget=1.0, fi_trials=0, run_fi=False, pass_flags=BASE)

cn, tn = opcount(H.BUILD_DIR / "sut_plumb_native.ll")
cp, tp = opcount(H.BUILD_DIR / "sut_plumb_prot.ll")
print(f"\n总指令  native={tn}  protected={tp}  ratio={tp/tn:.2f}x  added={tp-tn}")
cats = {
    "冗余计算 fadd/fmul/fsub(含vec)": ["fadd", "fmul", "fsub", "fdiv", "fneg"],
    "校验:比较 fcmp/icmp": ["fcmp", "icmp"],
    "校验:fabs/select": ["call", "select"],
    "签名 xor/bitcast": ["xor", "bitcast"],
    "管道 extract/insert/shuffle": ["extractelement", "insertelement", "shufflevector"],
    "转换 fptrunc/fpext": ["fptrunc", "fpext"],
    "访存 load/store": ["load", "store"],
}
print(f"{'类别':34}{'native':>8}{'prot':>8}{'added':>8}{'%of_added':>10}")
added_total = tp - tn
for name, ops in cats.items():
    n = sum(cn.get(o, 0) for o in ops); p = sum(cp.get(o, 0) for o in ops)
    print(f"{name:34}{n:>8}{p:>8}{p-n:>8}{100*(p-n)/added_total:>9.1f}%")
