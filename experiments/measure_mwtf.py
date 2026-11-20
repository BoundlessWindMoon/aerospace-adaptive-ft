#!/usr/bin/env python3
"""A2: MWTF / MEBF 表（当前策略 + 自适应 + 硬件锁步），λ(t) 加权后处理。

口径说明：
- MWTF = 完成的工作量 / 遭遇的(静默)失效数 (Reis ISCA'05)。慢(高开销)=单位墙钟完成的任务少，
  故障率不变 → MWTF 低；这正是"把更慢和更对合成一个可比的数"。
- 失效率(每墙钟秒) ∝ Σ_phase f_phase · λ_phase · AVF_{policy,phase}，与代码速度无关(曝光按墙钟计)。
- 工作率(吞吐) ∝ Σ_phase f_phase / overhead_{policy,phase}。
- AVF 取各故障类别等权聚合 SDC(SASSIFI 惯例)；失效定义=静默数据损坏(SDC)，DUE 另算(fail-safe)。
- 数据源：results/e2e_struct.csv（当前策略、已含 P0-4 修正的注入口径）；轨道 λ 取 atft_core。
硬件锁步(DCLS)：硬件捕获→DUE，静默 SDC=0，固定 2× 半吞吐(与 measure_cnn_capstone 一致)。
"""
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import atft_core as C

ROOT = Path(__file__).resolve().parent.parent
E2E = ROOT / "results" / "e2e_struct_aero.csv"   # 稳定命名(航天核;避免被 OBPMark 跑覆盖)

# load/store 数据属 ECC 域(SWIFT SoR 之外),软件 FT 不声明保护 → 从保护聚合口径剔除。
# addr 保留:它是地址**计算**错(访问到错位置),ECC 管不了,是软件层的活。
ECC_DELEGATED = {"load"}

F_SAA = C.LEO_SAA_FRAC
F_CALM = 1.0 - F_SAA
LAM_SAA = C.LAM_LEO_SAA
LAM_CALM = C.LAM_LEO_QUIET
LOCKSTEP_OV = 2.0


def load_e2e():
    """返回 {config: (mean_sdc_frac, overhead)}，SDC 按类别等权（剔除 ECC 域的 load/store）。"""
    agg = {}
    with open(E2E) as f:
        for r in csv.DictReader(f):
            if r["category"] in ECC_DELEGATED:
                continue
            cfg = r["config"]
            agg.setdefault(cfg, {"sdc": [], "ov": float(r["overhead"])})
            agg[cfg]["sdc"].append(float(r["sdc_pct"]) / 100.0)
    return {c: (sum(v["sdc"]) / len(v["sdc"]), v["ov"]) for c, v in agg.items()}


def mwtf(phases):
    """phases: [(f, lam, avf, ov), ...]；返回 (failure_rate, work_rate, mwtf)."""
    fail = sum(f * lam * avf for (f, lam, avf, ov) in phases)
    work = sum(f / ov for (f, lam, avf, ov) in phases)
    return fail, work, (work / fail if fail > 0 else float("inf"))


def main():
    e = load_e2e()
    p_nat, _ = e["native"]
    p_str, ov_str = e["struct_full"]
    gen = e.get("old_intTMR")  # 通用 TMR 参照(若在表里)

    # 每个方案给出两相(calm/saa)的 (f, lambda, AVF, overhead)
    scen = {}
    scen["native"]       = [(F_CALM, LAM_CALM, p_nat, 1.0), (F_SAA, LAM_SAA, p_nat, 1.0)]
    scen["struct_static"]= [(F_CALM, LAM_CALM, p_str, ov_str), (F_SAA, LAM_SAA, p_str, ov_str)]
    # 自适应：平静期关保护(走 native AVF/全吞吐)，SAA 期开结构栈
    scen["adaptive"]     = [(F_CALM, LAM_CALM, p_nat, 1.0), (F_SAA, LAM_SAA, p_str, ov_str)]
    # 硬件锁步：静默 SDC=0(硬件捕获→DUE)，固定 2×
    scen["hw_lockstep"]  = [(F_CALM, LAM_CALM, 0.0, LOCKSTEP_OV), (F_SAA, LAM_SAA, 0.0, LOCKSTEP_OV)]
    if gen:
        scen["generic_tmr"] = [(F_CALM, LAM_CALM, gen[0], gen[1]), (F_SAA, LAM_SAA, gen[0], gen[1])]

    fr_nat, wr_nat, mw_nat = mwtf(scen["native"])

    order = ["native", "generic_tmr", "struct_static", "adaptive", "hw_lockstep"]
    print(f"轨道: f_SAA={F_SAA:.3f} λ_SAA={LAM_SAA:.2e} λ_calm={LAM_CALM:.2e} (比 {LAM_SAA/LAM_CALM:.0f}×)")
    print(f"等权聚合 SDC: native={p_nat*100:.2f}% struct={p_str*100:.2f}%"
          + (f" generic={gen[0]*100:.2f}%" if gen else ""))
    print(f"\n{'方案':14}{'SDC%':>7}{'吞吐(rel)':>11}{'失效率(rel)':>13}{'MWTF(rel)':>12}  说明")
    rows = []
    notes = {"native": "裸奔基线", "generic_tmr": "通用TMR,全程付",
             "struct_static": "结构栈全程开", "adaptive": "平静关/SAA开",
             "hw_lockstep": "DCLS,静默0但半吞吐"}
    avf_disp = {"native": p_nat, "generic_tmr": gen[0] if gen else 0,
                "struct_static": p_str, "adaptive": p_str, "hw_lockstep": 0.0}
    for name in order:
        if name not in scen:
            continue
        fr, wr, mw = mwtf(scen[name])
        wr_rel = wr / wr_nat
        fr_rel = (fr / fr_nat) if fr_nat > 0 else float("inf")
        mw_rel = (mw / mw_nat) if mw != float("inf") else float("inf")
        mw_s = "∞(SDC=0)" if mw == float("inf") else f"{mw_rel:.1f}×"
        print(f"{name:14}{avf_disp[name]*100:>7.2f}{wr_rel:>11.2f}{fr_rel:>13.3f}{mw_s:>12}  {notes[name]}")
        rows.append({"policy": name, "agg_sdc_pct": round(avf_disp[name]*100, 3),
                     "throughput_rel": round(wr_rel, 3), "failure_rate_rel": round(fr_rel, 4),
                     "mwtf_rel": ("inf" if mw == float("inf") else round(mw_rel, 2)),
                     "note": notes[name]})
    outp = ROOT / "results" / "mwtf_mebf.csv"
    with open(outp, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print(f"\nCSV -> {outp}")
    print("读法: MWTF(rel) 越高越好=单位静默失效下完成的工作越多。自适应用近 native 吞吐换到")
    print("      结构栈级别的失效率(SAA 期保护)，故 MWTF 高于全程静态栈;锁步静默为 0 但永半吞吐。")


if __name__ == "__main__":
    main()
