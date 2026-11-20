#!/usr/bin/env python3
"""防御面静态指令占比：保护码指令 / 原始程序指令，按函数(内核)拆分。

读取 measure_authenticity.py 已产出的 build/sut_auth_{native,exact_tmr,approx_simd}.ll。
- 原始指令：native 模块里每个函数的指令数(native 模式不插保护码)。
- 保护指令：变换后同名函数里携带保护元数据的指令数。
防御面 = 保护指令 / 原始指令，越小越好。按函数看能暴露"结构匹配→小防御面"的差异。
"""
import re
from pathlib import Path

BUILD = Path(__file__).resolve().parent.parent / "build"

PROT_TAGS = [
    "atft.voter.internal", "atft.replica", "atft.abft",
    "atft.cmp.sig", "atft.int.phi", "atft.int.ld", "atft.int.sh",
    "atft.addr.sig", "atft.intd.sig", "atft.res.sig", "atft.lpa.sig",
    "atft.ldr.sig", "atft.ldr.ld", "atft.dmr.ld", "atft.dmr.phi",
    "atft.dmr.sig", "atft.ok.flag",
]
PROT_RE = re.compile(r"!(" + "|".join(t.replace(".", r"\.") for t in PROT_TAGS) + r")\b")
# 指令行：赋值形 `%x = ...` 或无返回值助记符；注意末尾不能用 \b(赋值分支以 = 结尾)
INSTR_RE = re.compile(r"^\s+(%\S+\s*=\s|(store|br|ret|call|switch|unreachable|"
                      r"fence|cmpxchg|atomicrmw|resume|indirectbr)[\s(])")
DEF_RE = re.compile(r"^define\s+.*?@([A-Za-z0-9_.$]+)\s*\(")

# 展示用的关键内核(函数名)
FOCUS = [
    ("bench_pb_gemm",                 "GEMM (matmul, ABFT 天然适配)"),
    ("bench_cnn_conv_abft",           "CNN conv + ABFT(Freivalds)"),
    ("bench_cnn_classifier_decision", "端到端分类器(决策)"),
    ("bench_gnc_kalman",              "GNC Kalman(反馈自纠)"),
]


def count_by_func(path):
    funcs, cur, tot, prot = {}, None, 0, 0
    for line in Path(path).read_text().splitlines():
        s = line.rstrip()
        m = DEF_RE.match(s)
        if m:
            cur = m.group(1); funcs[cur] = [0, 0]; continue
        if s == "}":
            cur = None; continue
        if cur and INSTR_RE.match(s):
            funcs[cur][0] += 1
            if PROT_RE.search(s):
                funcs[cur][1] += 1
    return funcs


def main():
    nat = count_by_func(BUILD / "sut_auth_native.ll")
    tmr = count_by_func(BUILD / "sut_auth_exact_tmr.ll")
    sim = count_by_func(BUILD / "sut_auth_approx_simd.ll")
    abft_path = BUILD / "sut_auth_abft.ll"
    abf = count_by_func(abft_path) if abft_path.exists() else {}
    struct_path = BUILD / "sut_auth_struct.ll"
    stc = count_by_func(struct_path) if struct_path.exists() else {}

    def agg(d):
        return sum(v[0] for v in d.values()), sum(v[1] for v in d.values())

    print("=== 防御面全套聚合 (budget=1.0, 全程保护, 44 内核) ===")
    nt, _ = agg(nat)
    rows = [("native", nat), ("exact_tmr", tmr), ("approx_simd", sim)]
    if abf:
        rows.append(("abft+exclusive", abf))
    if stc:
        rows.append(("struct_full+ABFT(部署)", stc))
    print("  说明：防御面(加法) = (变换后指令总数 − native) / native，这是真实的代码/攻击面增量；")
    print("        tag 口径只数带保护元数据的指令，会漏掉残差/地址/SIMD lane 等机制，仅作下界参考。")
    for name, d in rows:
        t, p = agg(d)
        added = (t - nt) / nt * 100.0 if nt else 0.0
        print(f"  {name:22} 指令总数={t:>6}  防御面(加法)={added:>6.1f}%  "
              f"(tag下界={100.0*p/nt:>5.1f}%)  总膨胀={t/nt:.2f}x")

    print("\n=== 关键内核逐个 (防御面 = 保护指令 / 该内核 native 指令) ===")
    hdr = f"{'内核':34}{'native':>8}{'TMR面':>8}{'SIMD面':>9}"
    if abf:
        hdr += f"{'ABFT面':>9}"
    print(hdr)
    for fn, desc in FOCUS:
        n0 = nat.get(fn, [0, 0])[0]
        if n0 == 0:
            continue
        tp = tmr.get(fn, [0, 0])[1]
        sp = sim.get(fn, [0, 0])[1]
        line = f"{desc:34}{n0:>8}{100.0*tp/n0:>7.0f}%{100.0*sp/n0:>8.0f}%"
        if abf:
            ap = abf.get(fn, [0, 0])[1]
            line += f"{100.0*ap/n0:>8.0f}%"
        print(line)
    print("\n防御面越小越好。ABFT 把矩阵类内核的逐指令三取二换成 O(n²) 代数校验，")
    print("静态指令防御面应显著收缩(GEMM/2mm/3mm 走 ABFT 分支、不再逐指令复制)。")


if __name__ == "__main__":
    main()
