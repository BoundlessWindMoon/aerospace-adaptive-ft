#!/usr/bin/env python3
"""从冻结基线 results/rq1_*–rq9_*.csv/json 渲染分章节报告（对回 rq 文件，可核验）。

用法：python3 tools/build_report.py   → tools/reports/full_report.md
"""
import csv
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(ROOT, "results")

POL = [("native", "无保护"), ("swift", "SWIFT"), ("eddi", "EDDI"),
       ("lltfi_sid", "LLTFI-SID"), ("exact_tmr", "精确三模"),
       ("approx_scalar", "近似三模·标量"), ("approx_simd", "近似三模·向量"),
       ("adaptive_high", "自适应·高档")]
CAT = [("fp64", "浮点64"), ("fp32", "浮点32"), ("int_arith", "整数"),
       ("cast", "类型转换"), ("load", "访存"), ("addr", "地址"),
       ("cmp_branch", "比较分支")]
SUITE = {"pb_": "PolyBench", "parsec_": "PARSEC", "dsp_": "KissFFT",
         "obpmark_": "OBPMark", "coast_": "COAST", "axc_": "loop-perf"}


def load_csv(name):
    with open(os.path.join(RES, name)) as f:
        return list(csv.DictReader(f))


def load_json(name):
    with open(os.path.join(RES, name)) as f:
        return json.load(f)


def suite_of(b):
    for k, v in SUITE.items():
        if b.startswith(k):
            return v
    return "?"


def fnum(x, d=2):
    try:
        return f"{float(x):.{d}f}"
    except (ValueError, TypeError):
        return str(x)
def sec_methods():
    d = load_json("rq1_aggregate.json")
    L = ["## 一、方法对比（等权跨故障类别）", "",
         "| 方法 | 指令倍率 | SDC% | DUE% | 检出恢复% |", "| --- | --- | --- | --- | --- |"]
    for k, lab in POL:
        if k not in d:
            continue
        e = d[k]["equal_category"]
        L.append(f"| {lab} | {e['inst_ratio']:.2f} | {e['sdc']:.2f} | "
                 f"{e['due']:.2f} | {e['det_rec']:.2f} |")
    L.append("")
    return L


def sec_category():
    d = load_json("rq1_aggregate.json")
    L = ["## 二、逐故障类别 SDC%（方法 × 类别）", "",
         "| 方法 | " + " | ".join(l for _, l in CAT) + " |",
         "| " + " | ".join(["---"] * (len(CAT) + 1)) + " |"]
    for k, lab in POL:
        if k not in d:
            continue
        bc = d[k]["by_category"]
        vals = [f"{bc[c]['sdc']:.1f}" if c in bc else "—" for c, _ in CAT]
        L.append(f"| {lab} | " + " | ".join(vals) + " |")
    L.append("")
    return L


def sec_perbench():
    rows = [r for r in load_csv("rq1_fault_injection_outcomes.csv")
            if float(r["trials"] or 0) > 0]
    agg = {}
    for r in rows:
        key = (r["policy"], r["benchmark"])
        a = agg.setdefault(key, {"sdc": [], "due": [], "dr": [], "ir": []})
        a["sdc"].append(float(r["sdc_pct"])); a["due"].append(float(r["due_pct"]))
        a["dr"].append(float(r["det_recovered_pct"]))
        a["ir"].append(float(r["norm_hw_inst_ratio"]))
    benches = sorted({b for (_, b) in agg})

    def mean(key, f):
        v = agg.get(key, {}).get(f, [])
        return sum(v) / len(v) if v else 0.0

    L = ["## 三、逐负载详细（无保护 vs 近似三模·向量，等权跨类别）", "",
         "| 负载 | 套件 | 无保护SDC% | 无保护DUE% | 本方法SDC% | 本方法检出恢复% | 指令倍率 |",
         "| --- | --- | --- | --- | --- | --- | --- |"]
    for b in benches:
        L.append(f"| {b} | {suite_of(b)} | {mean(('native', b), 'sdc'):.1f} | "
                 f"{mean(('native', b), 'due'):.1f} | "
                 f"{mean(('approx_simd', b), 'sdc'):.1f} | "
                 f"{mean(('approx_simd', b), 'dr'):.1f} | "
                 f"{mean(('approx_simd', b), 'ir'):.1f} |")
    L.append("")
    return L
def sec_bitfield():
    rows = load_csv("rq5_bitfield_sensitivity.csv")
    nat = {r["bit_field"]: r for r in rows if r["policy"] == "native"}
    ap = {r["bit_field"]: r for r in rows if r["policy"] == "approx_simd"}
    L = ["## 四、翻转位域敏感性（浮点 double）", "",
         "| 翻转位域 | 无保护SDC% | 近似三模SDC% | 近似三模检出恢复% |",
         "| --- | --- | --- | --- |"]
    for bf in ["Sign (Bit 63)", "Exponent (Bits 52-62)",
               "Mantissa_High (Bits 32-51)", "Mantissa_Low (Bits 0-31)"]:
        if bf in nat and bf in ap:
            L.append(f"| {bf} | {float(nat[bf]['sdc_pct']):.1f} | "
                     f"{float(ap[bf]['sdc_pct']):.1f} | "
                     f"{float(ap[bf]['det_recovered_pct']):.1f} |")
    L.append("")
    return L


def sec_mbu():
    rows = load_csv("rq6_mbu_burst_stress.csv")
    by = {}
    for r in rows:
        by[(r["policy"], r["stress_level"])] = r
    levels = []
    for r in rows:
        if r["stress_level"] not in levels:
            levels.append(r["stress_level"])

    def fail(pol, lv):
        r = by.get((pol, lv))
        return (float(r["sdc_pct"]) + float(r["due_pct"])) if r else 0.0

    def sdc(pol, lv):
        r = by.get((pol, lv))
        return float(r["sdc_pct"]) if r else 0.0

    def due(pol, lv):
        r = by.get((pol, lv))
        return float(r["due_pct"]) if r else 0.0

    L = ["## 五、多比特 / 连发故障（失效% = SDC + DUE）", "",
         "| 故障形态 | 无保护SDC% | SWIFT DUE% | 精确三模失效% | 近似三模失效% |",
         "| --- | --- | --- | --- | --- |"]
    for lv in levels:
        L.append(f"| {lv} | {sdc('native', lv):.1f} | {due('swift', lv):.1f} | "
                 f"{fail('exact_tmr', lv):.2f} | {fail('approx_simd', lv):.2f} |")
    L.append("")
    return L


def sec_threshold():
    rows = load_csv("rq3_delta_threshold_sensitivity.csv")
    agg = {}
    for r in rows:
        a = agg.setdefault(r["delta"], {"fa": [], "dr": [], "sdc": []})
        a["fa"].append(float(r["false_alarm_pct_no_fault"]))
        a["dr"].append(float(r["fi_det_recovered_pct"]))
        a["sdc"].append(float(r["fi_sdc_pct"]))
    order = ["1e-07", "1e-05", "5e-05", "0.001", "0.05", "0.5"]
    L = ["## 六、表决阈值校准（近似三模，等权跨负载）", "",
         "| 阈值 δ | 无故障误报% | 检出恢复% | SDC% |", "| --- | --- | --- | --- |"]
    for dl in order:
        if dl not in agg:
            continue
        a = agg[dl]
        n = len(a["fa"])
        L.append(f"| {dl} | {sum(a['fa'])/n:.2f} | {sum(a['dr'])/n:.2f} | "
                 f"{sum(a['sdc'])/n:.2f} |")
    L.append("")
    return L


def sec_orbit():
    rows = [r for r in load_csv("rq9_temporal_mttf.csv")
            if r["environment"] == "sampex_orbit_avg" and r["fault_space_mib"] == "4.0"]
    order = {k: lab for k, lab in POL}
    L = ["## 七、SAMPEX 轨道平均可靠性（4 MiB 故障空间）", "",
         "| 方法 | 指令倍率 | MTTF(小时) | MWTF(相对无保护) | 每任务失效数(90天) |",
         "| --- | --- | --- | --- | --- |"]
    for pol in ["native", "swift", "exact_tmr", "approx_simd"]:
        r = next((x for x in rows if x["policy"] == pol), None)
        if not r:
            continue
        mttf = r.get("mttf_hours") or "—"
        try:
            mttf = f"{float(mttf):.3g}"
        except ValueError:
            pass
        L.append(f"| {order.get(pol, pol)} | {fnum(r['inst_ratio'])} | {mttf} | "
                 f"{fnum(r.get('mwtf_lb95_vs_native', '—'), 1)} | "
                 f"{fnum(r.get('failures_per_mission', '—'), 2)} |")
    L.append("")
    return L


def sec_riscv():
    rows = load_csv("rq8_riscv_rvv_evaluation.csv")
    nat = {r["benchmark"]: r for r in rows if r["policy"] == "native"}
    ap = {r["benchmark"]: r for r in rows if r["policy"] == "approx_simd"}
    L = ["## 八、RISC-V (RV64GCV) 评估", "",
         "RVV 向量 op 数 > 0 表示该负载被向量化；QEMU 下注入对比无保护与近似三模。", "",
         "| 负载 | RVV向量op | 无保护 QEMU-SDC% | 近似三模 QEMU-SDC% | 近似三模检出恢复% |",
         "| --- | --- | --- | --- | --- |"]
    for b in sorted(nat):
        rn = nat[b]
        ra = ap.get(b, {})
        L.append(f"| {b} | {rn.get('rv64gcv_rvv_vec_ops', '—')} | "
                 f"{fnum(rn.get('qemu_fi_sdc_pct', '—'), 1)} | "
                 f"{fnum(ra.get('qemu_fi_sdc_pct', '—'), 1)} | "
                 f"{fnum(ra.get('qemu_fi_det_recovered_pct', '—'), 1)} |")
    L.append("")
    return L


def sec_todo():
    return ["## 九、已知局限", "",
            "- 整数/类型转换/地址/控制流四类几乎未覆盖；控制流是无保护下失效率最高的一类。",
            "- 阈值当前为单一全局值，未做逐负载校准。",
            "- 各方法注入的故障点集合随方法变化，非严格同一组。",
            "- 全程单种子，重复运行方差未测。", ""]


def main():
    L = ["# 自适应容错框架 — 实验报告（数据源 results/rq1–rq9）", ""]
    for fn in (sec_methods, sec_category, sec_perbench, sec_bitfield,
               sec_mbu, sec_threshold, sec_orbit, sec_riscv, sec_todo):
        L += fn()
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "reports", "full_report.md")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(L) + "\n")
    print(f"[写入] {out}")


if __name__ == "__main__":
    main()



