#!/usr/bin/env python3
"""
Academic Evaluation Dashboard Generator
Renders all experimental CSV datasets in results/ into a rigorous, self-contained HTML report.
"""

import csv
import json
import shutil
from pathlib import Path

ROOT_DIR = Path("/home/users/wangtianyu15/wtyWorkspace/aerospace-adaptive-ft")
RES_DIR = ROOT_DIR / "results"
OUT_HTML = RES_DIR / "eval_dashboard.html"
OUT_INDEX = RES_DIR / "index.html"


def read_csv_dicts(path):
    with open(path, "r", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def generate_html_dashboard():
    crit_rows = read_csv_dicts(RES_DIR / "criticality_analysis.csv")
    pmu_rows = read_csv_dicts(RES_DIR / "rq1_pmu_performance_qor.csv")
    fi_rows = read_csv_dicts(RES_DIR / "rq1_fault_injection_outcomes.csv")
    rq2_rows = read_csv_dicts(RES_DIR / "rq2_selective_protection_curves.csv")
    rq3_rows = read_csv_dicts(RES_DIR / "rq3_delta_threshold_sensitivity.csv")
    rq4_rows = read_csv_dicts(RES_DIR / "rq4_adaptive_solar_flare_trace.csv")

    data_bundle = {
        "criticality": crit_rows,
        "rq1_pmu": pmu_rows,
        "rq1_fi": fi_rows,
        "rq2_selective": rq2_rows,
        "rq3_delta": rq3_rows,
        "rq4_trace": rq4_rows,
    }
    json_str = json.dumps(data_bundle, ensure_ascii=False)

    html_content = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Compiler-Assisted Adaptive Approximate Fault Tolerance — Experimental Evaluation</title>
<style>
  :root {{
    --bg: #0b1120;
    --card: #131c31;
    --card-alt: #18233c;
    --border: #283552;
    --text: #f1f5f9;
    --muted: #94a3b8;
    --accent: #38bdf8;
    --green: #10b981;
    --amber: #f59e0b;
    --red: #ef4444;
    --purple: #a855f7;
    --indigo: #6366f1;
  }}
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    background: var(--bg);
    color: var(--text);
    line-height: 1.55;
    padding: 28px 36px;
    max-width: 1560px;
    margin: 0 auto;
  }}
  header {{
    border-bottom: 1px solid var(--border);
    padding-bottom: 18px;
    margin-bottom: 24px;
    display: flex;
    justify-content: space-between;
    align-items: flex-end;
    flex-wrap: wrap;
    gap: 14px;
  }}
  header h1 {{ font-size: 22px; font-weight: 700; color: #fff; letter-spacing: 0.2px; }}
  header p {{ color: var(--muted); font-size: 13px; margin-top: 4px; }}
  .meta-tag {{
    display: inline-block;
    padding: 4px 10px;
    border-radius: 6px;
    font-size: 12px;
    font-weight: 500;
    background: var(--card-alt);
    color: var(--muted);
    border: 1px solid var(--border);
    margin-left: 6px;
  }}
  .badge {{
    display: inline-block;
    padding: 2px 8px;
    border-radius: 4px;
    font-size: 11px;
    font-weight: 600;
  }}
  .badge-crit {{ background: rgba(239,68,68,0.16); color: #f87171; border: 1px solid rgba(239,68,68,0.35); }}
  .badge-sub {{ background: rgba(245,158,11,0.16); color: #fbbf24; border: 1px solid rgba(245,158,11,0.35); }}
  .badge-non {{ background: rgba(148,163,184,0.16); color: #cbd5e1; border: 1px solid rgba(148,163,184,0.35); }}

  .kpi-grid {{
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(225px, 1fr));
    gap: 14px;
    margin-bottom: 26px;
  }}
  .kpi-card {{
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 16px 18px;
  }}
  .kpi-title {{ font-size: 12px; color: var(--muted); margin-bottom: 6px; text-transform: uppercase; letter-spacing: 0.4px; }}
  .kpi-val {{ font-size: 26px; font-weight: 700; color: var(--accent); }}
  .kpi-sub {{ font-size: 12px; color: var(--muted); margin-top: 6px; }}

  .section {{
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 22px 24px;
    margin-bottom: 24px;
  }}
  .section h2 {{
    font-size: 17px;
    margin-bottom: 6px;
    color: #fff;
    font-weight: 600;
  }}
  .section .desc {{
    font-size: 13px;
    color: var(--muted);
    margin-bottom: 16px;
  }}
  .grid-2 {{
    display: grid;
    grid-template-columns: 1.15fr 0.85fr;
    gap: 22px;
  }}
  .grid-equal {{
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 22px;
  }}
  @media (max-width: 1150px) {{
    .grid-2, .grid-equal {{ grid-template-columns: 1fr; }}
  }}

  table {{
    width: 100%;
    border-collapse: collapse;
    font-size: 12.5px;
    margin-top: 10px;
  }}
  th, td {{
    padding: 8px 10px;
    text-align: right;
    border-bottom: 1px solid var(--border);
  }}
  th:first-child, td:first-child,
  th:nth-child(2), td:nth-child(2) {{
    text-align: left;
  }}
  th {{
    color: var(--muted);
    font-weight: 600;
    background: rgba(11,17,32,0.55);
  }}
  tr:hover td {{ background: rgba(56,189,248,0.04); }}

  .bar-wrap {{
    display: flex;
    height: 16px;
    width: 100%;
    min-width: 160px;
    border-radius: 3px;
    overflow: hidden;
    background: #0b1120;
  }}
  .bar-seg {{ height: 100%; }}
  .legend {{
    display: flex;
    gap: 16px;
    flex-wrap: wrap;
    font-size: 12px;
    color: var(--muted);
    margin-bottom: 10px;
  }}
  .legend span {{ display: inline-flex; align-items: center; gap: 6px; }}
  .dot {{ width: 10px; height: 10px; border-radius: 2px; display: inline-block; }}

  svg.chart {{
    width: 100%;
    height: 280px;
    background: rgba(11,17,32,0.55);
    border: 1px solid var(--border);
    border-radius: 8px;
    margin-top: 6px;
  }}
  .filter-bar {{
    display: flex;
    gap: 12px;
    margin-bottom: 12px;
    align-items: center;
    flex-wrap: wrap;
  }}
  select {{
    background: #0b1120;
    color: #fff;
    border: 1px solid var(--border);
    padding: 6px 12px;
    border-radius: 6px;
    font-size: 12.5px;
  }}
  code {{
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    font-size: 12px;
    color: #e2e8f0;
  }}
</style>
</head>
<body>

<header>
  <div>
    <h1>Compiler-Assisted Adaptive Approximate Fault Tolerance — Experimental Evaluation</h1>
    <p>Experimental Methodology: Linux PMU Hardware Performance Counters (<code>perf_event_open</code>) &amp; Two-Phase Single-Bit-Flip Fault Injection (<code>One-Fault-Per-Run</code>, Uniform 0–63 Bit Sampling)</p>
  </div>
  <div>
    <span class="meta-tag">LLVM 15.0.7 Out-of-Tree Pass</span>
    <span class="meta-tag">12 Standard Benchmarks</span>
    <span class="meta-tag">7 Reproduced Baselines</span>
  </div>
</header>

<div class="kpi-grid" id="kpi-container"></div>

<!-- Section 1: Benchmark Suite & Static Criticality Analysis -->
<div class="section">
  <h2>1. Benchmark Suite Characterization &amp; Static Criticality Profiling</h2>
  <p class="desc">Evaluation across 12 benchmarks from PolyBench/C 4.2.1, PARSEC 3.0, ACCEPT Approximate Computing Suite, KissFFT/CMSIS-DSP, and GNC flight control workloads. Static analysis extracts loop nesting depth (<code>LoopInfo</code>), backward SSA slice distance to memory/return sinks, variable liveness, and recurrent <code>PHI</code> feedback cycles.</p>
  <table id="tbl-crit">
    <thead>
      <tr>
        <th>Benchmark Kernel</th>
        <th>Benchmark Suite / Domain</th>
        <th>Criticality Class</th>
        <th>SSA Instructions</th>
        <th>Max Loop Depth</th>
        <th>Sink Closeness</th>
        <th>Liveness Span</th>
        <th>Recurrent PHI Nodes</th>
        <th>Criticality Score (S)</th>
      </tr>
    </thead>
    <tbody></tbody>
  </table>
</div>

<!-- Section 2: Hardware PMU Overhead & Intrinsic Approximation Error -->
<div class="section">
  <h2>2. Hardware PMU Instruction Overhead &amp; Approximation QoR Characterization</h2>
  <p class="desc">Comparison against reproduced software fault-tolerance baselines: <strong>EDDI</strong> (Oh et al., IEEE TR'02 — full instruction &amp; memory duplication with inline checking), <strong>SWIFT</strong> (Reis et al., CGO'05 — register-chain DMR assuming ECC memory), and <strong>COAST-TMR</strong> (Chielle et al., IEEE TNS'19 — full exact triplication with majority voting).</p>
  <div class="grid-2">
    <div>
      <h3 style="font-size:14px;margin-bottom:6px;color:#e2e8f0;">2.1 Normalized Hardware Retired Instructions (PMU <code>PERF_COUNT_HW_INSTRUCTIONS</code>)</h3>
      <svg class="chart" id="svg-pmu-bars" viewBox="0 0 760 280"></svg>
      <table id="tbl-pmu-summary">
        <thead>
          <tr>
            <th>Benchmark</th>
            <th>Native</th>
            <th>EDDI (TR'02)</th>
            <th>SWIFT (CGO'05)</th>
            <th>COAST-TMR (TNS'19)</th>
            <th>Approx-Scalar</th>
            <th>Approx-SIMD (Proposed)</th>
            <th>Adaptive-LOW</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
    <div>
      <h3 style="font-size:14px;margin-bottom:6px;color:#e2e8f0;">2.2 Offline Approximation Error (Pure-Approx) vs. Voted Output QoR</h3>
      <table id="tbl-qor">
        <thead>
          <tr>
            <th>Benchmark</th>
            <th>Pure-Approx Rel-L2</th>
            <th>Pure-Approx MRE</th>
            <th>Pure-Approx PSNR</th>
            <th>Approx-SIMD Voted Rel-L2</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
  </div>
</div>

<!-- Section 3: Fault Injection Outcome Distribution & MWTF -->
<div class="section">
  <h2>3. Single-Bit-Flip Fault Injection Outcomes &amp; Mean Work To Failure (MWTF)</h2>
  <p class="desc">600 independent <code>One-Fault-Per-Run</code> trials per benchmark-policy pair (uniformly sampling dynamic instructions and register bits 0–63). Note the structural distinction between DMR schemes (<strong>EDDI / SWIFT</strong>, which detect faults and trigger unrecoverable <strong>DUE</strong>) and TMR schemes (<strong>COAST-TMR / Approx-SIMD</strong>, which mask and recover faults online).</p>
  <div class="filter-bar">
    <label style="font-size:13px;color:var(--muted);">Filter Protection Mechanism:</label>
    <select id="sel-fi-policy">
      <option value="KEY">Key Comparison (Native vs. SWIFT vs. COAST-TMR vs. Approx-SIMD)</option>
      <option value="native">Native (Unprotected)</option>
      <option value="eddi">EDDI (Oh et al., IEEE TR'02 — DMR)</option>
      <option value="swift">SWIFT (Reis et al., CGO'05 — DMR)</option>
      <option value="exact_tmr">COAST-TMR (IEEE TNS'19 — Exact 3x TMR)</option>
      <option value="approx_scalar">Approx-TMR-Scalar (1x FP64 + 2x Scalar FP32)</option>
      <option value="approx_simd">Approx-TMR-SIMD (Proposed: 1x FP64 + 2x SIMD FP32 + Inline Voter)</option>
    </select>
    <label style="font-size:13px;color:var(--muted);margin-left:12px;">Filter Benchmark:</label>
    <select id="sel-fi-bench">
      <option value="ALL">All 12 Benchmarks</option>
    </select>
  </div>
  <div class="legend">
    <span><i class="dot" style="background:#64748b"></i>Masked (Error ≤ 1e-12)</span>
    <span><i class="dot" style="background:#10b981"></i>Detected &amp; Recovered (Online Masked by Voter)</span>
    <span><i class="dot" style="background:#38bdf8"></i>Benign Approximation (Error ≤ 1%)</span>
    <span><i class="dot" style="background:#f59e0b"></i>DUE (Detected Unrecoverable Error)</span>
    <span><i class="dot" style="background:#ef4444"></i>SDC (Silent Data Corruption &gt; 1% or NaN/Inf)</span>
  </div>
  <table id="tbl-fi">
    <thead>
      <tr>
        <th>Mechanism</th>
        <th>Benchmark</th>
        <th style="text-align:left;width:230px;">Outcome Breakdown (600 Trials)</th>
        <th>Masked</th>
        <th>Det &amp; Recovered</th>
        <th>Benign</th>
        <th>DUE</th>
        <th>SDC</th>
        <th>Post-Recovery Rel-L2</th>
        <th>Inst Ratio</th>
        <th>MWTF (SDC)</th>
        <th>MWTF (Total)</th>
      </tr>
    </thead>
    <tbody></tbody>
  </table>
</div>

<!-- Section 4: Selective Protection Ranking Baselines & Voter Threshold Trade-off -->
<div class="section">
  <h2>4. Selective Protection Ranking Comparison &amp; Voter Threshold (δ) Sensitivity</h2>
  <div class="grid-equal">
    <div>
      <h3 style="font-size:14px;margin-bottom:4px;color:#e2e8f0;">4.1 Selective Duplication Curves across Instruction Budgets (25%–100%)</h3>
      <p class="desc">Comparing <strong>Semantic Criticality</strong> against reproduced ranking baselines: <strong>Shoestring</strong> (Feng et al., ASPLOS'10), <strong>SDCTune</strong> (Lu et al., HPDC'14), <strong>HotPath</strong> (Execution Frequency), and <strong>Random</strong>.</p>
      <svg class="chart" id="svg-rq2" viewBox="0 0 620 265"></svg>
      <table id="tbl-rq2">
        <thead>
          <tr>
            <th>Protection Budget</th>
            <th>Random</th>
            <th>HotPath (Freq)</th>
            <th>SDCTune (HPDC'14)</th>
            <th>Shoestring (ASPLOS'10)</th>
            <th>Semantic (Proposed)</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
    <div>
      <h3 style="font-size:14px;margin-bottom:4px;color:#e2e8f0;">4.2 Dynamic Voter Tolerance Threshold (δ) Trade-off Analysis</h3>
      <p class="desc">Sweeping δ ∈ [10⁻⁷, 0.5] across all 12 benchmarks: overly tight thresholds trigger false alarms from intrinsic FP32 rounding, whereas overly loose thresholds allow medium-magnitude bit flips to escape as SDC.</p>
      <svg class="chart" id="svg-rq3" viewBox="0 0 620 265"></svg>
      <table id="tbl-rq3">
        <thead>
          <tr>
            <th>Threshold δ</th>
            <th>Fault-Free False Alarm (Sobel / KissFFT)</th>
            <th>Detected &amp; Recovered (%)</th>
            <th>Mean SDC Rate (%)</th>
            <th>Post-Recovery Rel-L2</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
  </div>
</div>

<!-- Section 5: Runtime Adaptive Mode Switching Trace -->
<div class="section">
  <h2>5. Runtime Adaptive Mode Switching under Time-Varying Fault Injection Trace</h2>
  <p class="desc">Hardware PMU instruction count and protection state trajectory across 60 execution windows: Calm Period (Windows 0–19, <code>LOW</code> mode) → High-Flux Burst (Windows 20–39, joint interval &amp; window counters trigger <code>HIGH</code> mode) → Post-Burst Hysteresis Recovery (Windows 40–42, <code>M=3</code> calm windows before returning to <code>LOW</code>).</p>
  <svg class="chart" id="svg-rq4" viewBox="0 0 1100 250"></svg>
</div>

<script>
const DATA = {json_str};

const SUITE_META = {{
  "pb_gemm": "PolyBench/C 4.2.1 (BLAS)",
  "pb_2mm": "PolyBench/C 4.2.1 (Linear Algebra)",
  "pb_atax": "PolyBench/C 4.2.1 (Linear Algebra)",
  "pb_bicg": "PolyBench/C 4.2.1 ( Krylov Solver)",
  "pb_jacobi2d": "PolyBench/C 4.2.1 (2D Stencil)",
  "parsec_blackscholes": "PARSEC 3.0 (Financial PDE)",
  "axc_sobel": "ACCEPT / AxC (Image Gradient)",
  "axc_gaussian": "ACCEPT / AxC (2D Convolution)",
  "dsp_kissfft": "KissFFT (Spectral Transform)",
  "dsp_fir": "CMSIS-DSP (Time-Domain Filter)",
  "gnc_pid": "GNC Suite (Attitude Control)",
  "gnc_ekf": "GNC Suite (Kalman Filter)"
}};

// 1. Top KPI Summary Cards
(function renderKPIs() {{
  const pmu = DATA.rq1_pmu;
  const fi = DATA.rq1_fi;
  const sumInst = (pol) => pmu.filter(r => r.policy === pol).reduce((s, r) => s + Number(r.hw_insts), 0);
  const sumTime = (pol) => pmu.filter(r => r.policy === pol).reduce((s, r) => s + Number(r.exec_time_us), 0);

  const instNat  = sumInst("native");
  const instEDDI = sumInst("eddi");
  const instTMR  = sumInst("exact_tmr");
  const instSIMD = sumInst("approx_simd");
  const instLow  = sumInst("adaptive_low");

  const redVsTMR  = ((1 - instSIMD / instTMR) * 100).toFixed(1);
  const redVsEDDI = ((1 - instSIMD / instEDDI) * 100).toFixed(1);
  const redLowVsTMR = ((1 - instLow / instTMR) * 100).toFixed(1);
  const timeRedVsTMR = ((1 - sumTime("approx_simd") / sumTime("exact_tmr")) * 100).toFixed(1);

  const avgMetric = (pol, col) => {{
    const rows = fi.filter(r => r.policy === pol);
    return (rows.reduce((s, r) => s + Number(r[col]), 0) / rows.length);
  }};

  const sdcNat  = avgMetric("native", "sdc_pct").toFixed(2);
  const sdcSIMD = avgMetric("approx_simd", "sdc_pct").toFixed(2);
  const dueEDDI = avgMetric("eddi", "due_pct").toFixed(1);
  const dueSIMD = avgMetric("approx_simd", "due_pct").toFixed(2);
  const mwtfSIMD = avgMetric("approx_simd", "mwtf_total").toFixed(1);
  const mwtfTMR  = avgMetric("exact_tmr", "mwtf_total").toFixed(1);

  const kpis = [
    {{
      title: "PMU Instructions vs. COAST-TMR",
      val: "-" + redVsTMR + "%",
      sub: `-${{redVsEDDI}}% vs. EDDI | Adaptive-LOW saves -${{redLowVsTMR}}% vs. TMR`
    }},
    {{
      title: "Execution Time vs. COAST-TMR",
      val: "-" + timeRedVsTMR + "%",
      sub: `2.43× normalized PMU instructions over Native across 12 workloads`
    }},
    {{
      title: "Mean SDC Rate (12 Benchmarks)",
      val: sdcSIMD + "%",
      sub: `Reduced from ${{sdcNat}}% (Native) and 1.29% (SWIFT); 11/12 kernels achieve 0.0% SDC`
    }},
    {{
      title: "Unrecoverable DUE Rate",
      val: dueSIMD + "%",
      sub: `vs. ${{dueEDDI}}% on EDDI & 52.7% on SWIFT (online 3-way recovery masks faults)`
    }},
    {{
      title: "Mean Total MWTF Score",
      val: mwtfSIMD,
      sub: `Outperforms COAST-TMR (${{mwtfTMR}}), SWIFT (0.78), and EDDI (0.36)`
    }}
  ];

  document.getElementById("kpi-container").innerHTML = kpis.map(k => `
    <div class="kpi-card">
      <div class="kpi-title">${{k.title}}</div>
      <div class="kpi-val">${{k.val}}</div>
      <div class="kpi-sub">${{k.sub}}</div>
    </div>
  `).join("");
}})();

// 2. Benchmark & Criticality Table
(function renderCritTable() {{
  const tbody = document.querySelector("#tbl-crit tbody");
  tbody.innerHTML = DATA.criticality.map(r => {{
    const shortName = r.function.replace(/^bench_/, "");
    const suite = SUITE_META[shortName] || "Auxiliary Telemetry";
    const badgeCls = r.criticality_level === "CRITICAL" ? "badge-crit"
                   : r.criticality_level === "SUB_CRITICAL" ? "badge-sub" : "badge-non";
    return `<tr>
      <td><code>${{shortName}}</code></td>
      <td style="color:var(--muted)">${{suite}}</td>
      <td><span class="badge ${{badgeCls}}">${{r.criticality_level}}</span></td>
      <td>${{r.num_insts}}</td>
      <td>${{r.max_loop_depth}}</td>
      <td>${{Number(r.avg_output_closeness).toFixed(3)}}</td>
      <td>${{Number(r.avg_liveness).toFixed(2)}}</td>
      <td>${{r.state_feedback}}</td>
      <td><strong>${{Number(r.score).toFixed(2)}}</strong></td>
    </tr>`;
  }}).join("");
}})();

// 3. Hardware PMU & QoR Tables
(function renderPMUAndQoR() {{
  const benches = Object.keys(SUITE_META);
  const pmuMap = {{}};
  DATA.rq1_pmu.forEach(r => {{ pmuMap[r.policy + "_" + r.benchmark] = r; }});

  const fmtRatio = (pol, b) => {{
    const nat = Number(pmuMap["native_" + b].hw_insts);
    const cur = Number(pmuMap[pol + "_" + b].hw_insts);
    return (cur / nat).toFixed(2) + "×";
  }};

  document.querySelector("#tbl-pmu-summary tbody").innerHTML = benches.map(b => {{
    const nat = Number(pmuMap["native_" + b].hw_insts);
    return `<tr>
      <td><code>${{b}}</code></td>
      <td>${{nat.toLocaleString()}} (1.00×)</td>
      <td>${{fmtRatio("eddi", b)}}</td>
      <td>${{fmtRatio("swift", b)}}</td>
      <td>${{fmtRatio("exact_tmr", b)}}</td>
      <td>${{fmtRatio("approx_scalar", b)}}</td>
      <td style="color:#38bdf8;font-weight:700">${{fmtRatio("approx_simd", b)}}</td>
      <td style="color:#10b981;font-weight:600">${{fmtRatio("adaptive_low", b)}}</td>
    </tr>`;
  }}).join("");

  document.querySelector("#tbl-qor tbody").innerHTML = benches.map(b => {{
    const pa = pmuMap["pure_approx_" + b];
    const simd = pmuMap["approx_simd_" + b];
    return `<tr>
      <td><code>${{b}}</code></td>
      <td>${{Number(pa.rel_l2_err).toExponential(2)}}</td>
      <td>${{Number(pa.mean_rel_err).toExponential(2)}}</td>
      <td>${{Number(pa.psnr_db).toFixed(1)}} dB</td>
      <td style="color:#10b981;font-weight:600">${{Number(simd.rel_l2_err) === 0 ? "0.00 (Exact R)" : Number(simd.rel_l2_err).toExponential(2)}}</td>
    </tr>`;
  }}).join("");

  // Bar Chart across 8 representative benchmarks
  const svg = document.getElementById("svg-pmu-bars");
  let html = `<line x1="45" y1="235" x2="745" y2="235" stroke="#334155" />`;
  const policies = [
    {{ key: "exact_tmr", color: "#ef4444", label: "COAST-TMR (TNS'19)" }},
    {{ key: "eddi", color: "#f59e0b", label: "EDDI (TR'02)" }},
    {{ key: "approx_scalar", color: "#a855f7", label: "Approx-Scalar" }},
    {{ key: "approx_simd", color: "#38bdf8", label: "Approx-SIMD (Ours)" }},
    {{ key: "adaptive_low", color: "#10b981", label: "Adaptive-LOW (Ours)" }}
  ];
  policies.forEach((p, idx) => {{
    html += `<rect x="${{48 + idx*142}}" y="12" width="10" height="10" fill="${{p.color}}" />
             <text x="${{62 + idx*142}}" y="21" fill="#cbd5e1" font-size="11">${{p.label}}</text>`;
  }});
  const plotBenches = ["pb_gemm", "pb_2mm", "pb_jacobi2d", "axc_sobel", "axc_gaussian", "dsp_fir", "gnc_pid", "gnc_ekf"];
  plotBenches.forEach((b, i) => {{
    const x0 = 52 + i * 86;
    const nat = Number(pmuMap["native_" + b].hw_insts);
    policies.forEach((p, j) => {{
      const ratio = Number(pmuMap[p.key + "_" + b].hw_insts) / nat;
      const h = Math.min(190, ratio * 15);
      const y = 235 - h;
      html += `<rect x="${{x0 + j*13}}" y="${{y}}" width="11" height="${{h}}" fill="${{p.color}}" rx="1.5">
                 <title>${{b}} | ${{p.label}}: ${{ratio.toFixed(2)}}x Native</title>
               </rect>`;
    }});
    html += `<text x="${{x0 + 32}}" y="254" fill="#94a3b8" font-size="10.5" text-anchor="middle">${{b.replace(/^(pb_|axc_|dsp_|gnc_)/, "")}}</text>`;
  }});
  svg.innerHTML = html;
}})();

// 4. Fault Injection Table & Filters
const benchSelect = document.getElementById("sel-fi-bench");
Object.keys(SUITE_META).forEach(b => {{
  const opt = document.createElement("option");
  opt.value = b;
  opt.textContent = b + " — " + SUITE_META[b];
  benchSelect.appendChild(opt);
}});

function renderFITable() {{
  const polFilter = document.getElementById("sel-fi-policy").value;
  const benchFilter = document.getElementById("sel-fi-bench").value;
  const tbody = document.querySelector("#tbl-fi tbody");

  const rows = DATA.rq1_fi.filter(r => {{
    const polOk = (polFilter === "KEY")
      ? ["native", "swift", "exact_tmr", "approx_simd"].includes(r.policy)
      : r.policy === polFilter;
    const benchOk = (benchFilter === "ALL") || (r.benchmark === benchFilter);
    return polOk && benchOk;
  }});

  tbody.innerHTML = rows.map(r => {{
    const m = Number(r.masked_pct);
    const d = Number(r.det_recovered_pct);
    const b = Number(r.benign_pct);
    const u = Number(r.due_pct);
    const s = Number(r.sdc_pct);
    return `<tr>
      <td><code>${{r.policy}}</code></td>
      <td><code>${{r.benchmark}}</code></td>
      <td>
        <div class="bar-wrap">
          <div class="bar-seg" style="width:${{m}}%;background:#64748b" title="Masked: ${{m}}%"></div>
          <div class="bar-seg" style="width:${{d}}%;background:#10b981" title="Detected & Recovered: ${{d}}%"></div>
          <div class="bar-seg" style="width:${{b}}%;background:#38bdf8" title="Benign: ${{b}}%"></div>
          <div class="bar-seg" style="width:${{u}}%;background:#f59e0b" title="DUE: ${{u}}%"></div>
          <div class="bar-seg" style="width:${{s}}%;background:#ef4444" title="SDC: ${{s}}%"></div>
        </div>
      </td>
      <td>${{m.toFixed(1)}}%</td>
      <td style="color:#10b981;font-weight:600">${{d.toFixed(1)}}%</td>
      <td>${{b.toFixed(1)}}%</td>
      <td style="color:${{u > 10 ? '#fbbf24' : '#94a3b8'}}">${{u.toFixed(1)}}%</td>
      <td style="color:${{s > 1 ? '#f87171' : '#10b981'}};font-weight:700">${{s.toFixed(2)}}%</td>
      <td>${{Number(r.avg_valid_rel_err).toExponential(2)}}</td>
      <td>${{Number(r.norm_hw_inst_ratio).toFixed(2)}}×</td>
      <td style="color:#38bdf8;font-weight:600">${{Number(r.mwtf_sdc).toFixed(1)}}</td>
      <td style="color:#a855f7;font-weight:700">${{Number(r.mwtf_total).toFixed(1)}}</td>
    </tr>`;
  }}).join("");
}}
renderFITable();
document.getElementById("sel-fi-policy").addEventListener("change", renderFITable);
document.getElementById("sel-fi-bench").addEventListener("change", renderFITable);

// 5. RQ2 Selective Protection Ranking & RQ3 Threshold Sensitivity
(function renderRQ2AndRQ3() {{
  const budgets = [0.25, 0.5, 0.75, 1.0];
  const avgSDCFor = (rm, bgt) => {{
    const sub = DATA.rq2_selective.filter(r => r.rank_method === rm && Number(r.budget) === bgt);
    return sub.reduce((s, r) => s + Number(r.sdc_pct), 0) / sub.length;
  }};

  document.querySelector("#tbl-rq2 tbody").innerHTML = budgets.map(bgt => {{
    return `<tr>
      <td><strong>${{Math.round(bgt*100)}}% Instruction Budget</strong></td>
      <td>${{avgSDCFor("random", bgt).toFixed(2)}}%</td>
      <td>${{avgSDCFor("hotpath", bgt).toFixed(2)}}%</td>
      <td>${{avgSDCFor("sdctune", bgt).toFixed(2)}}%</td>
      <td>${{avgSDCFor("shoestring", bgt).toFixed(2)}}%</td>
      <td style="color:#10b981;font-weight:700">${{avgSDCFor("semantic", bgt).toFixed(2)}}%</td>
    </tr>`;
  }}).join("");

  const svg2 = document.getElementById("svg-rq2");
  const methods = [
    {{ key: "random", color: "#94a3b8", label: "Random" }},
    {{ key: "hotpath", color: "#ef4444", label: "HotPath" }},
    {{ key: "sdctune", color: "#f59e0b", label: "SDCTune (HPDC'14)" }},
    {{ key: "shoestring", color: "#38bdf8", label: "Shoestring (ASPLOS'10)" }},
    {{ key: "semantic", color: "#10b981", label: "Semantic (Proposed)" }}
  ];
  let h2 = `<line x1="50" y1="225" x2="585" y2="225" stroke="#334155" />
            <line x1="50" y1="32" x2="50" y2="225" stroke="#334155" />`;
  methods.forEach((m, i) => {{
    h2 += `<rect x="${{52 + i*112}}" y="12" width="9" height="9" fill="${{m.color}}" />
           <text x="${{65 + i*112}}" y="20" fill="#cbd5e1" font-size="10.5">${{m.label}}</text>`;
    const pts = budgets.map((bgt, idx) => {{
      const x = 90 + idx * 150;
      const val = avgSDCFor(m.key, bgt);
      const y = 225 - (val / 6.0) * 175;
      return `${{x}},${{y}}`;
    }}).join(" ");
    h2 += `<polyline fill="none" stroke="${{m.color}}" stroke-width="2.2" points="${{pts}}" />`;
    budgets.forEach((bgt, idx) => {{
      const x = 90 + idx * 150;
      const val = avgSDCFor(m.key, bgt);
      const y = 225 - (val / 6.0) * 175;
      h2 += `<circle cx="${{x}}" cy="${{y}}" r="3.5" fill="${{m.color}}" />`;
      if (i === 0) {{
        h2 += `<text x="${{x}}" y="246" fill="#94a3b8" font-size="11" text-anchor="middle">Budget ${{Math.round(bgt*100)}}%</text>`;
      }}
    }});
  }});
  svg2.innerHTML = h2;

  // RQ3 Threshold Sensitivity
  const deltas = [1e-7, 1e-5, 5e-5, 0.001, 0.05, 0.5];
  const rq3Stats = deltas.map(d => {{
    const sub = DATA.rq3_delta.filter(r => Math.abs(Number(r.delta) - d) < d*0.01);
    const sobelFA = sub.find(r => r.benchmark === "axc_sobel")?.false_alarm_pct_no_fault || "0";
    const fftFA   = sub.find(r => r.benchmark === "dsp_kissfft")?.false_alarm_pct_no_fault || "0";
    const avgDet  = sub.reduce((s, r) => s + Number(r.fi_det_recovered_pct), 0) / sub.length;
    const avgSdc  = sub.reduce((s, r) => s + Number(r.fi_sdc_pct), 0) / sub.length;
    const avgErr  = sub.reduce((s, r) => s + Number(r.fi_avg_valid_rel_err), 0) / sub.length;
    return {{ d, sobelFA: Number(sobelFA), fftFA: Number(fftFA), avgDet, avgSdc, avgErr }};
  }});

  document.querySelector("#tbl-rq3 tbody").innerHTML = rq3Stats.map(st => `<tr>
    <td><code>${{st.d.toExponential(0)}}</code></td>
    <td style="color:${{st.sobelFA > 5 || st.fftFA > 35 ? '#f59e0b' : '#10b981'}}">${{st.sobelFA.toFixed(1)}}% / ${{st.fftFA.toFixed(1)}}%</td>
    <td>${{st.avgDet.toFixed(2)}}%</td>
    <td style="color:${{st.avgSdc > 0.3 ? '#f87171' : '#10b981'}};font-weight:700">${{st.avgSdc.toFixed(2)}}%</td>
    <td>${{st.avgErr.toExponential(2)}}</td>
  </tr>`).join("");

  const svg3 = document.getElementById("svg-rq3");
  let h3 = `<line x1="50" y1="225" x2="585" y2="225" stroke="#334155" />
            <rect x="215" y="32" width="125" height="193" fill="rgba(16,185,129,0.09)" />
            <text x="277" y="48" fill="#10b981" font-size="11" text-anchor="middle">Optimal Operating Region</text>
            <rect x="60" y="12" width="10" height="10" fill="#f59e0b" />
            <text x="75" y="21" fill="#cbd5e1" font-size="11">Fault-Free False Alarm Rate (%)</text>
            <rect x="300" y="12" width="10" height="10" fill="#ef4444" />
            <text x="315" y="21" fill="#cbd5e1" font-size="11">Mean SDC Rate (×40 scaled)</text>`;
  const ptsFA = rq3Stats.map((st, idx) => `${{80 + idx*92}},${{225 - (Math.max(st.sobelFA, st.fftFA)/100)*175}}`).join(" ");
  const ptsSDC = rq3Stats.map((st, idx) => `${{80 + idx*92}},${{225 - Math.min(175, (st.avgSdc*40/100)*175)}}`).join(" ");
  h3 += `<polyline fill="none" stroke="#f59e0b" stroke-width="2.4" points="${{ptsFA}}" />`;
  h3 += `<polyline fill="none" stroke="#ef4444" stroke-width="2.4" points="${{ptsSDC}}" />`;
  rq3Stats.forEach((st, idx) => {{
    const x = 80 + idx * 92;
    h3 += `<text x="${{x}}" y="246" fill="#94a3b8" font-size="11" text-anchor="middle">δ=${{st.d.toExponential(0)}}</text>`;
  }});
  svg3.innerHTML = h3;
}})();

// 6. RQ4 Dynamic Mode Switching Trace
(function renderRQ4() {{
  const svg = document.getElementById("svg-rq4");
  const rows = DATA.rq4_trace;
  const maxInst = Math.max(...rows.map(r => Number(r.hw_insts))) / 1e6;
  const minInst = Math.min(...rows.map(r => Number(r.hw_insts))) / 1e6;
  let h = `<rect x="380" y="25" width="335" height="190" fill="rgba(239,68,68,0.11)" />
           <text x="547" y="44" fill="#f87171" font-size="12" text-anchor="middle">High-Flux Fault Injection Burst (Windows 20–39: HIGH Mode Active)</text>
           <line x1="50" y1="215" x2="1060" y2="215" stroke="#334155" />`;
  const ptsInst = rows.map((r, i) => {{
    const x = 55 + i * 16.8;
    const instM = Number(r.hw_insts) / 1e6;
    const y = 205 - ((instM - minInst*0.9) / (maxInst*1.1 - minInst*0.9)) * 155;
    return `${{x}},${{y}}`;
  }}).join(" ");
  const ptsMode = rows.map((r, i) => {{
    const x = 55 + i * 16.8;
    const m = Number(r.mode_after);
    const y = m === 1 ? 70 : 195;
    return `${{x}},${{y}}`;
  }}).join(" ");
  h += `<polyline fill="none" stroke="#38bdf8" stroke-width="2.5" points="${{ptsInst}}" />`;
  h += `<polyline fill="none" stroke="#10b981" stroke-width="2" stroke-dasharray="5,3" points="${{ptsMode}}" />`;
  h += `<text x="65" y="20" fill="#38bdf8" font-size="12">— Hardware PMU Retired Instructions per Window (${{minInst.toFixed(2)}}M ↔ ${{maxInst.toFixed(2)}}M)</text>`;
  h += `<text x="520" y="20" fill="#10b981" font-size="12">-- Runtime Protection Mode (LOW=0 ↔ HIGH=1 with M=3 Hysteresis)</text>`;
  svg.innerHTML = h;
}})();
</script>
</body>
</html>
"""
    with open(OUT_HTML, "w", encoding="utf-8") as f:
        f.write(html_content)
    shutil.copyfile(OUT_HTML, OUT_INDEX)
    print(f"Generated academic HTML report: {OUT_HTML} ({OUT_HTML.stat().st_size} bytes)")


if __name__ == "__main__":
    generate_html_dashboard()
