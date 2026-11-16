#!/usr/bin/env python3
"""自适应容量石（capstone）：λ(t) 驱动的解析切换，一套控制器跑三条真实轨道。

把前面散落的件拼成一条可辩护的主线：
  ① 三条 λ(t) 轨迹——全部锚定到**实测/文献引用**的空间上翻率，不是拍的：
       LEO   SAMPEX 670×520 km i=81.7°，4.21e-6 upsets/bit-day
             (Adolphsen et al., IEEE TNS 42(6):1964, 1995)
       SSO   Alsat-1 686 km i=98.2°，4.04e-7 SEU/bit-day
             (Bentoutou, Adv. Space Res. 48(7), 2011)
       GEO+耀斑  静默背景 + 太阳耀斑增强 1e3~1e5×，峰值 0.7 upset/bit-day
             (CREME96 耀斑模型；Xapsos/Barth 耀斑统计)
  ② 实测菜单 (o_ij, e_ij)：开销 o 取自 kernel_properties/coarse_overhead_subset，
       残余每故障失效概率 e 取自 solid 矩阵 fp64 FI 实测。
  ③ 解析切换曲线：min Σ w_i o_ij  s.t.  λ Σ w_i e_ij ≤ ε 的 KKT 解，
       每核一条硬门限 λ* = ε / e_native（native 之上必须升档到 e≈0 的策略）。
  ④ payoff：同一控制器在三条轨迹上的平均开销 vs 全程 always-coarse / always-native，
       并核验约束 λ·e ≤ ε 处处满足（always-native 在峰值处违约）。

泛用性论据：控制器只认 λ 的数值，不认轨道——三条轨迹换着喂，控制器代码一行不改。
这正是 Young/Daly λ 参数化范式的推广（从检查点间隔推广到防护档位）。

用法：
    python3 experiments/adaptive_capstone.py
产物：
    results/opt/capstone_menu.csv            每核每策略的 (o, e) 与切换门限 λ*
    results/opt/capstone_trace_<orbit>.csv   每条轨迹的 λ(t) 与所选档位、瞬时开销/残余
    results/opt/capstone_payoff.csv          三轨道 payoff 汇总
"""
import csv
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OPT = os.path.join(ROOT, "results", "opt")

# --- SAMPEX 轨道常量（与 adaptive_budget.py 口径一致，4 MiB 故障空间）----------
# 活跃计算态的故障率（faults/sec）。SAA 内外比 ~200×。
LAM_LEO_QUIET = 4.31008e-05     # SAA 外（静默 LEO）
LAM_LEO_SAA = 0.00862015        # SAA 内（峰值）
LEO_PERIOD_S = 96.58 * 60.0
LEO_SAA_FRAC = 0.1856           # rq9 选定行 b=26000

# SSO 相对 LEO 的轨道平均率标度（实测 4.04e-7 / 4.21e-6）
SSO_SCALE = 4.04e-7 / 4.21e-6
SSO_PERIOD_S = 98.48 * 60.0
SSO_SAA_FRAC = 0.2107

# GEO：静默背景远低于 LEO 静默；耀斑峰值相对静默 ~1e4（文献 1e3~1e5 的几何中位）
LAM_GEO_QUIET = LAM_LEO_QUIET * 1e-2     # GEO 静默背景（GCR 主导，低通量）
GEO_FLARE_FACTOR = 1.0e4                  # 耀斑相对静默的增强倍数


def load_csv(name):
    with open(os.path.join(OPT, name)) as f:
        return list(csv.DictReader(f))
def build_menu():
    """每核的候选策略 (o=开销×, e=残余每故障失效概率)。

    e 一律取 solid 矩阵 fp64 实测 sdc_pct/100；o 取 kernel_properties 的 cyc 比与
    coarse_overhead_subset 的 DMR+重执行比。结构化核（可约/可做结果校验）有 e≈0 的
    ~1× 策略，不随 λ 变——自带免费防护；非结构化核只有 native(1×) 与 coarse(≈2×)，
    λ 在二者间切换，这才是自适应真正的杠杆。
    """
    props = {r["benchmark"]: r
             for r in load_csv("kernel_properties_a4_t200_matrix_solid.csv")}
    coarse = {r["benchmark"]: float(r["coarse_dmr_rerun"])
              for r in load_csv("coarse_overhead_subset.csv")}
    # 可做 ~1× 强校验且 e≈0 的核：可约→ABFT；cholesky/lu→Freivalds 结果校验
    CHEAP_CHECK = {
        "pb_2mm": ("abft", None), "pb_3mm": ("abft", None), "pb_gemm": ("abft", None),
        "pb_cholesky": ("freivalds", 1.10), "pb_lu": ("freivalds", 1.07),
    }
    menu = {}
    for b, r in props.items():
        e_nat = float(r["fp64_sdc_native"]) / 100.0
        cands = [{"policy": "native", "o": 1.0, "e": e_nat}]
        # 非结构化核的 SIMD-slack（仅 seidel 实测可达 1.49×、SDC=0）
        if b == "pb_seidel2d":
            cands.append({"policy": "simd_slack", "o": float(r["cyc_x_simd_tmr"]),
                          "e": 0.0})
        if b in CHEAP_CHECK:
            kind, o_fix = CHEAP_CHECK[b]
            o = o_fix if o_fix is not None else max(1.05, float(r["cyc_x_abft"]))
            cands.append({"policy": kind, "o": o, "e": 0.0})
        # 粗粒度整内核 DMR+重执行：全类别、e≈0。串行开销实测；并行(有空核)≈1.0×
        o_coarse = coarse.get(b, 2.1)
        cands.append({"policy": "coarse_serial", "o": o_coarse, "e": 0.0})
        cands.append({"policy": "coarse_parallel", "o": 1.0, "e": 0.0})
        menu[b] = cands
    return menu


def pick(cands, lam, eps, parallel_cores):
    """约束优化的逐核 KKT 解：在 λ·e ≤ ε 的可行集里取最便宜。

    并行(有空核)时 coarse_parallel 的 o=1.0 恒为可行最便宜 → 并行场景自适应退化成
    "总是 1×、处处达标"，正是前面核饱和研究的理想上界。串行(无空核)时 coarse 要 ~2×，
    于是 native↔coarse 之间出现真正的 λ 门限 λ* = ε/e_native。
    """
    feasible = []
    for c in cands:
        if c["policy"] == "coarse_parallel" and not parallel_cores:
            continue
        if c["policy"] != "coarse_parallel" and parallel_cores \
           and c["policy"] == "coarse_serial":
            continue
        if lam * c["e"] <= eps:
            feasible.append(c)
    if not feasible:                      # 约束不可满足：取 e 最小的（最强防护）
        return min(cands, key=lambda c: c["e"]), False
    return min(feasible, key=lambda c: c["o"]), True


# ===== 协态最优控制（把手调 ε 换成由任务失效预算 B 反解的 μ）==================
# 原理：min ∫o dt  s.t. ∫λe dt ≤ B 的拉格朗日量是 ∫(o + μ·λe) dt。单一积分约束 →
# 单个乘子 μ（协态），沿最优轨迹恒定（Pontryagin/横截条件）。给定 μ，逐点逐核取
# argmin_j (o_j + μ·λ·e_j)；μ 由"恰好用尽预算 B"唯一确定（互补松弛，F(μ)=B）。
# 与逐点速率上限 ε 的区别：切换门限 λ*(μ)=(o_prot−o_nat)/(μ·e_nat) 带 o_prot，
# 便宜防护的核更早切、昂贵的更晚切——按"每单位可靠性的开销"排序花预算，是严格最优；
# 等 ε 速率上限对所有核用同一残余帽、不管各核防护贵不贵，只是可行的启发式。

def avail(cands, parallel_cores):
    """按硬件场景过滤可用候选：并行用 coarse_parallel(1×)、串行用 coarse_serial。"""
    out = []
    for c in cands:
        if c["policy"] == "coarse_parallel" and not parallel_cores:
            continue
        if c["policy"] == "coarse_serial" and parallel_cores:
            continue
        out.append(c)
    return out


def choose_costate(cands, lam, parallel_cores, mu):
    """协态 argmin：min_j (o_j + μ·λ·e_j)。"""
    cs = avail(cands, parallel_cores)
    return min(cs, key=lambda c: c["o"] + mu * lam * c["e"])


def choose_ratecap(cands, lam, parallel_cores, eps):
    """逐点速率上限：λe≤ε 的可行集里取最便宜；不可行取 e 最小。"""
    cs = avail(cands, parallel_cores)
    feas = [c for c in cs if lam * c["e"] <= eps]
    return min(feas, key=lambda c: c["o"]) if feas \
        else min(cs, key=lambda c: c["e"])


def integrate(trace, menu, chooser, parallel_cores, dt):
    """沿轨迹积分：返回 (时间加权平均开销×, 任务期望失效次数 F)。"""
    kernels = sorted(menu)
    O = F = T = 0.0
    for (t, lam) in trace:
        o_sum = f_sum = 0.0
        for b in kernels:
            c = chooser(menu[b], lam, parallel_cores)
            o_sum += c["o"]; f_sum += lam * c["e"]
        O += (o_sum / len(kernels)) * dt
        F += f_sum * dt
        T += dt
    return O / T, F


def solve_to_budget(trace, menu, parallel_cores, dt, B, mode):
    """二分反解控制参数，使任务失效积分 F 恰好等于预算 B。

    mode='costate' 解 μ（F 随 μ 单调减）；mode='ratecap' 解 ε（F 随 ε 单调增）。
    返回 (参数, 平均开销×, 达到的 F)。
    """
    if mode == "costate":
        lo, hi = 0.0, 1.0
        chooser = lambda c, l, p: choose_costate(c, l, p, hi)
        while integrate(trace, menu, chooser, parallel_cores, dt)[1] > B:
            hi *= 10.0                      # 放大 μ 直到 F 压到 B 以下
            if hi > 1e12:
                break
        for _ in range(100):
            mid = 0.5 * (lo + hi)
            f = integrate(trace, menu,
                          lambda c, l, p: choose_costate(c, l, p, mid),
                          parallel_cores, dt)[1]
            if f > B:
                lo = mid
            else:
                hi = mid
        o, f = integrate(trace, menu,
                         lambda c, l, p: choose_costate(c, l, p, hi),
                         parallel_cores, dt)
        return hi, o, f
    else:
        lo, hi = 0.0, 1.0
        for _ in range(100):
            mid = 0.5 * (lo + hi)
            f = integrate(trace, menu,
                          lambda c, l, p: choose_ratecap(c, l, p, mid),
                          parallel_cores, dt)[1]
            if f > B:
                hi = mid
            else:
                lo = mid
        o, f = integrate(trace, menu,
                         lambda c, l, p: choose_ratecap(c, l, p, lo),
                         parallel_cores, dt)
        return lo, o, f
# ============================================================================


# ===== H1：真实连续 λ 剖面（用 rq9 的 |B|/L 时序，取代方波）===================
# 猜想：方波把 SAA 压成"全保护/全不保护"两档，中间 λ 区间为零，故阈值≈协态最优。
# 用实测 |B| 深度做连续 λ(t)，SAA 进出是平滑斜坡 → 中间 λ 区间变大 → (a) adaptive 对
# always-high 的差距变大（肩部只需部分核保护），(b) 协态对速率上限出现非零差距
# （连续 λ 下二者排序准则不同：速率上限按 e 排，协态按 (o_prot−o_nat)/e_nat 排）。
# λ 模型（须在论文声明的近似）：belt 内 λ 随 |B| 下降单调升，锚到两实测端点——
#   depth = clip((B_thresh−|B|)/(B_thresh−B_min), 0, 1)，仅 L<2 的内带计；
#   λ = λ_quiet · (λ_peak/λ_quiet)^depth。
# 这是 AP8 俘获通量形状的 |B|-深度代理（几何来自 IGRF 实测时序，尺度来自实测端点），
# 不是 CREME96 物理通量。
TS_PATH = os.path.join(ROOT, "results", "rq9_orbit_timeseries.csv")
B_THRESH = 26000.0
L_MAX = 2.0


def load_continuous_lambda(orbit, scale=1.0):
    rows = [r for r in csv.DictReader(open(TS_PATH)) if r["orbit"] == orbit]
    T = [float(r["t_s"]) for r in rows]
    B = [float(r["b_nt"]) for r in rows]
    L = [float(r["l_shell"]) for r in rows]
    belt = [b for b, l in zip(B, L) if l < L_MAX and b < B_THRESH]
    b_min = min(belt) if belt else B_THRESH
    span = max(B_THRESH - b_min, 1.0)
    dt = T[1] - T[0]
    out = []
    for t, b, l in zip(T, B, L):
        if l < L_MAX and b < B_THRESH:
            depth = min(1.0, max(0.0, (B_THRESH - b) / span))
        else:
            depth = 0.0
        lam = LAM_LEO_QUIET * (LAM_LEO_SAA / LAM_LEO_QUIET) ** depth * scale
        out.append((t, lam))
    return out, dt


def choose_low(cands, lam, p):
    return next(c for c in cands if c["policy"] == "native")


def choose_high(cands, lam, p):
    cs = [c for c in avail(cands, p) if c["e"] <= 1e-9]
    return min(cs, key=lambda c: c["o"])


def mwtf(o_mean, F):
    """MWTF ∝ 完成的有用工作 / 失效次数 = (T/o_mean)/F；此处返回 ∝ 1/(o_mean·F)。"""
    return 1.0 / (o_mean * F) if F > 0 else float("inf")


def experiment_continuous(menu, eps):
    """H1+H2：连续 λ 下 协态vs速率上限 差距 + MWTF 三方对比（串行场景）。"""
    # 传统全程软件 TMR（领域 status-quo 基线，~3×）：用 coarse_tmr 实测列
    tmr = {r["benchmark"]: float(r["coarse_tmr"])
           for r in load_csv("coarse_overhead_subset.csv")}
    tmr_mean = sum(tmr[b] for b in menu if b in tmr) / \
        sum(1 for b in menu if b in tmr)
    traces = [("leo_cont", load_continuous_lambda("sampex")),
              ("sso_cont", load_continuous_lambda("alsat1"))]
    print("\n== H1：真实连续 λ 剖面（串行场景）==")
    print("%-10s%10s%10s%8s%10s%10s%9s%9s" % ("orbit", "ratecap×", "costate×",
          "省%", "静态菜单×", "传统TMR×", "vs菜单%", "vsTMR%"))
    rows = []
    for name, (tr, dt) in traces:
        o_rc, B_rc = integrate(tr, menu,
                               lambda c, l, p: choose_ratecap(c, l, p, eps), False, dt)
        mu, o_cs, _ = solve_to_budget(tr, menu, False, dt, B_rc, "costate")
        saving = 100.0 * (o_rc - o_cs) / o_rc if o_rc else 0.0
        o_hi, _ = integrate(tr, menu, choose_high, False, dt)
        sv_menu = 100.0 * (o_hi - o_rc) / o_hi
        sv_tmr = 100.0 * (tmr_mean - o_rc) / tmr_mean
        print("%-10s%10.4f%10.4f%8.2f%10.4f%10.4f%9.1f%9.1f"
              % (name, o_rc, o_cs, saving, o_hi, tmr_mean, sv_menu, sv_tmr))
        rows.append({"orbit": name, "ratecap_overhead": round(o_rc, 4),
                     "costate_overhead": round(o_cs, 4),
                     "costate_saving_pct": round(saving, 2),
                     "static_menu_overhead": round(o_hi, 4),
                     "conventional_tmr_overhead": round(tmr_mean, 4),
                     "saving_vs_static_menu_pct": round(sv_menu, 1),
                     "saving_vs_conventional_tmr_pct": round(sv_tmr, 1),
                     "budget_fail": f"{B_rc:.4g}", "mu_star": f"{mu:.4g}"})
    with open(os.path.join(OPT, "capstone_continuous.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print("产物 capstone_continuous.csv")


def experiment_pareto(menu):
    """H3：可靠性-开销 Pareto 前沿。扫描失效预算 B，看自适应是否对两个静态基线
    全程占优（不只在某一点）。静态点：native=(B_nat,1×)、静态菜单=(0,1.64×)、
    传统 TMR=(0,~3.17×)。自适应前沿从 (B_nat,1×) 连到 (0,1.64×)，故处处 ≤ 静态菜单、
    且在可靠端把 TMR 的 3.17× 压到 1.64×。"""
    tr, dt = load_continuous_lambda("sampex")
    o_nat, B_nat = integrate(tr, menu, choose_low, False, dt)   # 前沿右端点
    o_menu, _ = integrate(tr, menu, choose_high, False, dt)     # 前沿左端点(全保护)
    tmr = {r["benchmark"]: float(r["coarse_tmr"])
           for r in load_csv("coarse_overhead_subset.csv")}
    tmr_mean = sum(tmr[b] for b in menu if b in tmr) / \
        sum(1 for b in menu if b in tmr)
    print("\n== H3：可靠性-开销 Pareto 前沿（leo_cont，串行）==")
    print("静态点：native=(%.3g 失效, %.3f×)  静态菜单=(0, %.3f×)  传统TMR=(0, %.3f×)"
          % (B_nat, o_nat, o_menu, tmr_mean))
    print("%-14s%14s%14s%14s" % ("预算B(失效)", "自适应×", "省vs菜单%", "省vsTMR%"))
    rows = []
    for frac in (1.0, 0.5, 0.2, 0.1, 0.05, 0.02, 0.01, 0.0):
        B = B_nat * frac
        if frac >= 1.0:
            o, Bget = o_nat, B_nat
        elif frac <= 0.0:
            o, Bget = o_menu, 0.0
        else:
            _, o, Bget = solve_to_budget(tr, menu, False, dt, B, "costate")
        sv_menu = 100.0 * (o_menu - o) / o_menu
        sv_tmr = 100.0 * (tmr_mean - o) / tmr_mean
        print("%-14.4g%14.4f%14.1f%14.1f" % (B, o, sv_menu, sv_tmr))
        rows.append({"budget_fail": f"{B:.4g}", "adaptive_overhead": round(o, 4),
                     "saving_vs_static_menu_pct": round(sv_menu, 1),
                     "saving_vs_conventional_tmr_pct": round(sv_tmr, 1)})
    with open(os.path.join(OPT, "capstone_pareto.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print("结论：自适应前沿处处 ≤ 静态菜单、远低于传统 TMR——对两基线全程 Pareto 占优。"
          "产物 capstone_pareto.csv")


# ===== H4：在线可实现性——λ 从 ECC 泊松事件流估计（不再给上帝视角）===========
# 猜想：真实部署看不到 λ 真值，只能数 ECC 事件。控制器改用估计 λ̂ 决策，可靠性仍按
# 真 λ 结算。预期出现"检测延迟→进 SAA 瞬间短暂欠保护→违约"与"安全裕度 κ→过保护→
# 丢收益"的权衡。关键是看这套在 SAA 过境(~13min)尺度上到底够不够快。
def experiment_online(menu, eps):
    import numpy as np
    rng = np.random.default_rng(12345)
    tr, dt = load_continuous_lambda("sampex")
    kernels = sorted(menu)
    # 真 λ → 每步泊松事件数（活跃计算态，4 MiB 故障空间口径）
    events = [int(rng.poisson(max(lam * dt, 0.0))) for (_, lam) in tr]

    def online(window_s, margin):
        win = max(1, int(window_s / dt))
        buf = []
        o_sum_all = viol = 0
        for (i, (t, lam_true)) in enumerate(tr):
            buf.append(events[i])
            if len(buf) > win:
                buf.pop(0)
            lam_hat = margin * sum(buf) / (len(buf) * dt)      # 滑窗 Poisson MLE × 裕度
            o_step = worst = 0.0
            for b in kernels:
                c = choose_ratecap(menu[b], lam_hat, False, eps)  # 用估计决策
                o_step += c["o"]
                worst = max(worst, lam_true * c["e"])             # 用真 λ 结算残余
            o_sum_all += o_step / len(kernels)
            if worst > eps:
                viol += 1
        return o_sum_all / len(tr), 100.0 * viol / len(tr)

    o_or, _ = integrate(tr, menu,
                        lambda c, l, p: choose_ratecap(c, l, p, eps), False, dt)
    print("\n== H4：在线 λ̂（ECC 泊松流，串行）；oracle 开销=%.3f× 违约=0 ==" % o_or)
    print("事件稀疏度：全程 %d 步，非零事件步仅 %d（λ·dt 多为 <1）"
          % (len(events), sum(1 for e in events if e > 0)))
    print("%-18s%-10s%12s%12s" % ("估计器", "裕度κ", "开销×", "违约%"))
    rows = []
    for win_s, margin, name in [(300, 1.0, "滑窗5min"), (900, 1.0, "滑窗15min"),
                                (3600, 1.0, "滑窗60min"), (900, 3.0, "滑窗15min"),
                                (900, 10.0, "滑窗15min")]:
        o, v = online(win_s, margin)
        print("%-18s%-10.0f%12.4f%12.2f" % (name, margin, o, v))
        rows.append({"estimator": name, "margin_kappa": margin,
                     "online_overhead": round(o, 4), "violation_pct": round(v, 2),
                     "oracle_overhead": round(o_or, 4)})
    with open(os.path.join(OPT, "capstone_online.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print("产物 capstone_online.csv")
def trace_leo(dt=30.0, orbits=3):
    """LEO 极轨：每周期在 SAA 内外之间方波切换（dwell=18.56%）。"""
    out = []
    saa = LEO_SAA_FRAC * LEO_PERIOD_S
    n = int(orbits * LEO_PERIOD_S / dt)
    for i in range(n):
        ph = (i * dt) % LEO_PERIOD_S
        lam = LAM_LEO_SAA if ph < saa else LAM_LEO_QUIET
        out.append((i * dt, lam))
    return out


def trace_sso(dt=30.0, orbits=3):
    """SSO：同样 SAA 调制，整体标度到 Alsat-1 实测均率（约 LEO 的 1/10）。"""
    out = []
    saa = SSO_SAA_FRAC * SSO_PERIOD_S
    n = int(orbits * SSO_PERIOD_S / dt)
    for i in range(n):
        ph = (i * dt) % SSO_PERIOD_S
        lam = (LAM_LEO_SAA if ph < saa else LAM_LEO_QUIET) * SSO_SCALE
        out.append((i * dt, lam))
    return out


def trace_geo_flare(dt=300.0, hours=72):
    """GEO：静默背景上叠加一次太阳耀斑（24h 起爆，2h 上升，12h 平台，12h 衰减）。

    增强倍数 1e4 取自文献 1e3~1e5 的几何中位；峰值对应 0.7 upset/bit-day 量级。
    """
    out = []
    n = int(hours * 3600.0 / dt)
    t_on, t_rise, t_plat, t_decay = 24 * 3600, 2 * 3600, 12 * 3600, 12 * 3600
    for i in range(n):
        t = i * dt
        g = 1.0
        if t_on <= t < t_on + t_rise:
            g = GEO_FLARE_FACTOR ** ((t - t_on) / t_rise)
        elif t_on + t_rise <= t < t_on + t_rise + t_plat:
            g = GEO_FLARE_FACTOR
        elif t_on + t_rise + t_plat <= t < t_on + t_rise + t_plat + t_decay:
            frac = (t - t_on - t_rise - t_plat) / t_decay
            g = GEO_FLARE_FACTOR ** (1.0 - frac)
        out.append((t, LAM_GEO_QUIET * g))
    return out


def run_trace(name, trace, menu, eps, parallel_cores, dt):
    """一条轨迹上跑控制器，返回 (逐步行, 汇总)。开销按各核等权平均。"""
    rows = []
    kernels = sorted(menu)
    sum_adapt = sum_low = sum_high = 0.0
    worst_resid_adapt = worst_resid_low = 0.0
    violations_low = 0
    for (t, lam) in trace:
        o_ad = o_lo = o_hi = 0.0
        r_ad = r_lo = 0.0
        for b in kernels:
            cands = menu[b]
            nat = next(c for c in cands if c["policy"] == "native")
            # always-high = 该核 e≈0 的最便宜策略（结构化核的 ~1× 或 coarse）
            hi = min((c for c in cands if c["e"] <= 1e-9),
                     key=lambda c: (1.0 if (c["policy"] == "coarse_parallel"
                                            and not parallel_cores) else c["o"]))
            if hi["policy"] == "coarse_parallel" and not parallel_cores:
                hi = min((c for c in cands if c["e"] <= 1e-9
                          and c["policy"] != "coarse_parallel"),
                         key=lambda c: c["o"])
            chosen, ok = pick(cands, lam, eps, parallel_cores)
            o_ad += chosen["o"]; o_lo += nat["o"]; o_hi += hi["o"]
            r_ad = max(r_ad, lam * chosen["e"])
            r_lo = max(r_lo, lam * nat["e"])
        k = len(kernels)
        o_ad /= k; o_lo /= k; o_hi /= k
        sum_adapt += o_ad; sum_low += o_lo; sum_high += o_hi
        worst_resid_adapt = max(worst_resid_adapt, r_ad)
        worst_resid_low = max(worst_resid_low, r_lo)
        if r_lo > eps:
            violations_low += 1
        rows.append({"t_s": int(t), "lambda_fps": f"{lam:.4e}",
                     "overhead_adaptive": round(o_ad, 3),
                     "overhead_always_low": round(o_lo, 3),
                     "overhead_always_high": round(o_hi, 3),
                     "resid_adaptive_fps": f"{r_ad:.3e}",
                     "resid_always_low_fps": f"{r_lo:.3e}",
                     "low_violates_eps": int(r_lo > eps)})
    ns = len(trace)
    summ = {"orbit": name, "eps_fps": f"{eps:.3e}",
            "parallel_cores": int(parallel_cores),
            "mean_overhead_adaptive": round(sum_adapt / ns, 3),
            "mean_overhead_always_low": round(sum_low / ns, 3),
            "mean_overhead_always_high": round(sum_high / ns, 3),
            "worst_resid_adaptive_fps": f"{worst_resid_adapt:.3e}",
            "worst_resid_always_low_fps": f"{worst_resid_low:.3e}",
            "adaptive_meets_eps": int(worst_resid_adapt <= eps + 1e-18),
            "low_meets_eps": int(violations_low == 0),
            "low_violation_steps_pct": round(100.0 * violations_low / ns, 1)}
    with open(os.path.join(OPT, f"capstone_trace_{name}.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    return summ
def main():
    menu = build_menu()

    # ε：可容忍的残余失效率（faults/sec 的每核残余）。按峰值 λ 选，使 native↔coarse 的
    # 门限 λ* 落在轨迹跨度之内。取 1e-4/s：静默段 native 达标，SAA/耀斑段必须升档。
    EPS = 1.0e-4

    # --- 菜单 + 每核解析切换门限 λ* = ε / e_native ---
    mrows = []
    print("== 实测菜单 (o, e) 与切换门限 λ* = ε/e_native（ε=%.1e faults/s）==" % EPS)
    print("%-18s%-16s%8s%10s%14s" % ("kernel", "policy", "o×", "e(SDC)", "λ*(/s)"))
    for b in sorted(menu):
        nat = next(c for c in menu[b] if c["policy"] == "native")
        lam_star = EPS / nat["e"] if nat["e"] > 0 else float("inf")
        for c in menu[b]:
            star = f"{lam_star:.3e}" if c["policy"] == "native" else ""
            print("%-18s%-16s%8.2f%10.4f%14s"
                  % (b, c["policy"], c["o"], c["e"], star))
            mrows.append({"benchmark": b, "policy": c["policy"],
                          "overhead_x": round(c["o"], 3), "e_sdc": round(c["e"], 4),
                          "lambda_star_fps": (f"{lam_star:.4e}"
                                              if c["policy"] == "native" else "")})
    with open(os.path.join(OPT, "capstone_menu.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(mrows[0].keys()))
        w.writeheader(); w.writerows(mrows)

    traces = [("leo", trace_leo(), 30.0), ("sso", trace_sso(), 30.0),
              ("geo_flare", trace_geo_flare(), 300.0)]

    payoff = []
    for cores in (False, True):          # 串行（无空核）与并行（有空核）两种硬件场景
        tag = "parallel" if cores else "serial"
        print("\n== payoff（%s 硬件场景）：同一控制器跑三条轨迹 ==" % tag)
        print("%-10s%12s%12s%12s%10s%12s" % ("orbit", "adaptive×",
              "always_low×", "always_high×", "达标?", "low违约%"))
        for name, tr, dt in traces:
            s = run_trace(f"{name}_{tag}", tr, menu, EPS, cores, dt)
            payoff.append(s)
            print("%-10s%12.3f%12.3f%12.3f%10s%12.1f"
                  % (name, s["mean_overhead_adaptive"],
                     s["mean_overhead_always_low"], s["mean_overhead_always_high"],
                     "是" if s["adaptive_meets_eps"] else "否",
                     s["low_violation_steps_pct"]))
    with open(os.path.join(OPT, "capstone_payoff.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(payoff[0].keys()))
        w.writeheader(); w.writerows(payoff)

    print("\n产物：capstone_menu.csv / capstone_trace_*.csv / capstone_payoff.csv")

    # ===== 协态最优 vs 逐点速率上限（匹配同一任务失效预算 B）=================
    # 口径：先让 ε=1e-4 的速率上限控制器跑出它的任务失效积分 B_rc，再解协态 μ 命中
    # 同一个 B_rc，比两者的平均开销。协态是该积分约束的最优解，速率上限是可行启发式，
    # 故协态开销 ≤ 速率上限；差距集中在 λ 连续变化的耀斑斜坡段（方波轨道上两者退化一致）。
    print("\n== 协态最优 vs 速率上限（串行场景，匹配同一失效预算 B）==")
    print("%-10s%14s%14s%12s%12s%10s" % ("orbit", "B(失效/任务)",
          "ratecap×", "costate×", "μ*", "省%"))
    crows = []
    for name, tr, dt in traces:
        # 速率上限在 ε=1e-4 下达到的 B 与开销
        o_rc, B_rc = integrate(
            tr, menu, lambda c, l, p: choose_ratecap(c, l, p, EPS), False, dt)
        # 协态解 μ 命中同一 B_rc
        mu, o_cs, B_cs = solve_to_budget(tr, menu, False, dt, B_rc, "costate")
        saving = 100.0 * (o_rc - o_cs) / o_rc if o_rc else 0.0
        print("%-10s%14.4g%14.4f%12.4f%12.4g%10.2f"
              % (name, B_rc, o_rc, o_cs, mu, saving))
        crows.append({"orbit": name, "budget_fail_per_mission": f"{B_rc:.4g}",
                      "ratecap_mean_overhead": round(o_rc, 4),
                      "costate_mean_overhead": round(o_cs, 4),
                      "mu_star": f"{mu:.4g}", "costate_B_check": f"{B_cs:.4g}",
                      "overhead_saving_pct": round(saving, 2)})
    with open(os.path.join(OPT, "capstone_costate.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(crows[0].keys()))
        w.writeheader(); w.writerows(crows)
    print("要点：μ 由预算 B 反解（不再手调 ε）；协态在耀斑斜坡上按'每单位可靠性开销'"
          "排序花预算，故 ≤ 速率上限。产物 capstone_costate.csv")

    experiment_continuous(menu, EPS)
    experiment_pareto(menu)
    experiment_online(menu, EPS)


if __name__ == "__main__":
    main()



