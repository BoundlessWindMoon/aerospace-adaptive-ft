#!/usr/bin/env python3
"""自适应容错实验的可复用核心原语（供 adaptive_capstone.py 及后续新实验 import）。

只放无副作用的纯原语：菜单构建、策略选择器、轨迹积分、协态/速率上限求解、λ 轨迹。
写文件、打印、拼装具体实验的逻辑留在各 experiment 脚本里。

新实验用法示例：
    from atft_core import build_menu, load_continuous_lambda, integrate, choose_ratecap
    menu = build_menu()
    tr, dt = load_continuous_lambda("sampex")
    o, F = integrate(tr, menu, lambda c, l, p: choose_ratecap(c, l, p, 1e-4), False, dt)
"""
import csv
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OPT = os.path.join(ROOT, "results", "opt")
TS_PATH = os.path.join(ROOT, "results", "rq9_orbit_timeseries.csv")
B_THRESH = 26000.0
L_MAX = 2.0

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

# ============================================================================
# 参考飞控平台 profile：RAD750（BAE Systems PowerPC 750）。
# 所有"非辐射"约束(算力/功耗/代码尺寸/deadline)的锚点，一律引公开 datasheet，
# 避免自设阈值导致 toy。辐射侧(λ) 已由上面 SAMPEX/Alsat-1 实测锚定。
# 来源：BAE RAD750 datasheet；Wikipedia RAD750；Aumictech 3U cPCI SBC spec；
#      ECSS-E-HB-20-40A（容错）；AIAA G002868（GNC 回路率）。
# 评估口径统一写：针对 RAD750-class 平台，参数取自上述公开 spec。
# ============================================================================
RAD750 = {
    "name": "RAD750 (BAE Systems, PowerPC 750)",
    "clock_mhz": 133.0,            # 110–200 MHz；取 133MHz（对应 CPU 5W 点）
    "dmips": 300.0,                # 266–435 DMIPS；标称 ~300
    "cpu_power_w": 5.0,            # CPU 功耗 @133MHz
    "sbc_power_w": 10.0,           # 整板 SBC 10–12.2W @198MHz
    "code_rom_kb": 256.0,          # 启动 SUROM 256KB（代码尺寸标尺）
    "sram_mb": 36.0,               # rad-hard SRAM
    "sdram_mb": 1024.0,            # EDAC SDRAM 1GB
    "tid_krad": (200.0, 1000.0),   # 0.25µm 200krad / 0.15µm 1Mrad
    "seu_err_bit_day": 1.6e-10,    # <1.6e-10 err/bit-day @90% worst-case GEO
    "temp_c": (-55.0, 125.0),
}

# 实时 deadline 锚点：ADCS/GNC 控制回路 1–10 Hz（AIAA G002868 综述；CubeSat GNC）。
# 取 10 Hz → 100 ms 作标称硬截止；星敏 4Hz；挠性控制 10–100Hz 为上界参考。
GNC_LOOP_HZ = 10.0
GNC_DEADLINE_S = 1.0 / GNC_LOOP_HZ        # 100 ms 硬截止

def rad750_time_s(base_time_s, overhead_x):
    """保护倍率 overhead_x 下的墙钟（相对无保护 base_time_s）。"""
    return base_time_s * overhead_x

def rad750_energy_j(time_s, power_w=None):
    """动态能耗 ≈ 时间 × 功率（同频同电压下，开销倍率≈能耗倍率）。"""
    return time_s * (power_w if power_w is not None else RAD750["cpu_power_w"])

def meets_deadline(time_s, deadline_s=None):
    return time_s <= (deadline_s if deadline_s is not None else GNC_DEADLINE_S)

# ============================================================================
# 现代平台(2024-2026 NASA/顶会实际在用,不再用 1990s-2010 老件)。两类都有向量单元:
#  - HPSC(NASA 下一代旗舰,RISC-V):SiFive X280×8,RVV1.0 512-bit 向量(可拼 4096-bit),
#    FP16/32/64,28,130 DMIPS@1GHz,256 GFLOPs 向量;rad-hard-by-design;
#    支持 DCLS 双核锁步(安全,吞吐减半) / Split 全吞吐(快,无锁步)两种模式。
#    来源:NASA cFS Symposium 2025、NASA HPSC 白皮书、SiFive X280 datasheet。
#  - Jetson Orin Nano(COTS 边缘,AMOS 2026/JPL 实测):6×ARM A78AE(NEON 128-bit)+
#    Ampere GPU(1024 CUDA/32 tensor),40 TOPS INT8;功率靠 nvpmodel 调频/关核(7W/15W)。
# 功率预算 = 调频/关核实现(DMIPS 随频率线性缩),这是真实的 degrade 执行器。
# ============================================================================
PLATFORMS = {
    "HPSC": {
        "cls": "space-modern", "isa": "RISC-V", "cores": 8, "mhz": 1000,
        "dmips": 28130, "vector_bits": 512, "gflops_vec": 256, "fp64": True,
        "power_w": None,  # datasheet 未公开干净 W(强调 100× perf/W);正式用查 PIC64-HPSC
        "modes": ["DCLS_lockstep(吞吐减半)", "split(全吞吐无锁步)"],
    },
    "JetsonOrinNano8G": {
        "cls": "cots-edge", "isa": "ARMv8.2-A78AE + Ampere GPU", "cores": 6,
        "mhz": 1500, "vector_bits": 128, "tops_int8": 40, "fp64": True,
        # nvpmodel 电源模式:功率→关核+限频(实测自 L4T r39.2 power 表)
        "power_modes": {"7W": {"cores": 4, "cpu_mhz": 960},
                        "15W": {"cores": 6, "cpu_mhz": 1500}},
    },
}

def platform_dmips(platform="HPSC", power_mode=None):
    """返回平台 DMIPS;给 Jetson 的 power_mode 则按关核+限频线性缩放。"""
    p = PLATFORMS[platform]
    if power_mode and "power_modes" in p:
        m = p["power_modes"][power_mode]
        full = p["dmips"] if "dmips" in p else p["cores"] * p["mhz"] * 8  # A78≈8 DMIPS/MHz
        return (m["cores"] * m["cpu_mhz"] * 8)  # 关核+限频后的标量 DMIPS
    return p.get("dmips", p["cores"] * p["mhz"] * 8)

def platform_time_s(hw_insts, platform="HPSC", power_mode=None):
    """实测动态指令数 → 该平台等效执行时间(s)。限频模式按 power_mode 缩放。"""
    return hw_insts / (platform_dmips(platform, power_mode) * 1e6)


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


def choose_low(cands, lam, p):
    return next(c for c in cands if c["policy"] == "native")


def choose_high(cands, lam, p):
    cs = [c for c in avail(cands, p) if c["e"] <= 1e-9]
    return min(cs, key=lambda c: c["o"])


def mwtf(o_mean, F):
    """MWTF ∝ 完成的有用工作 / 失效次数 = (T/o_mean)/F；此处返回 ∝ 1/(o_mean·F)。"""
    return 1.0 / (o_mean * F) if F > 0 else float("inf")


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
# ===== λ(t) 轨迹 ============================================================
# 真实连续剖面（AP8 俘获通量的 |B|-深度代理，须在论文声明非 CREME96）：
#   depth = clip((B_thresh−|B|)/(B_thresh−B_min), 0, 1)，仅 L<2 内带计；
#   λ = λ_quiet · (λ_peak/λ_quiet)^depth。几何来自 IGRF 实测时序，尺度来自实测端点。
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




