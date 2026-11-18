#!/usr/bin/env python3
"""屏障税 A/B：opt_i64 配置下 -atft-no-cse-barrier 开/关，
同时量 指令数(税) 与 SDC 覆盖(去屏障是否还防得住)。
结论用于判断"去屏障"是免费的还是会把冗余一起折叠掉。"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

OPT_I64 = ["-atft-cmp-tmr", "-atft-delta-runtime", "-atft-replica-exact",
           "-atft-int-tmr-wire", "-atft-int-tmr-i64"]
TRIALS = 120
TARGET = sys.argv[1] if len(sys.argv) > 1 else "fp64"

def run(tag, flags, mode="approx-simd", fi=True):
    sites = {}
    p, f = H.compile_and_evaluate_policy(
        SSA, tag, mode, budget=1.0, fi_trials=(TRIALS if fi else 0),
        run_fi=fi, fi_target_type=TARGET, pass_flags=flags,
        static_sites_out=sites)
    SITES[tag] = sites
    return p, f

SITES = {}

SSA = H.build_infrastructure()
cfgs = {
    "native":  ("native", []),
    "bar_on":  ("approx-simd", OPT_I64),
    "bar_off": ("approx-simd", OPT_I64 + ["-atft-no-cse-barrier"]),
}
pmu, fi = {}, {}
for tag, (mode, flags) in cfgs.items():
    p, f = run(tag, flags, mode=mode, fi=True)
    pmu[tag] = {r["benchmark"]: r for r in p}
    fi[tag]  = {r["benchmark"]: r for r in f}

print("\n==== BARRIER A/B (insts tax vs native | SDC%) ====")
print(f"{'bench':22} {'nat_i':>9} {'on_i':>9} {'off_i':>9} {'on_tax':>7} {'off_tax':>7} "
      f"{'nat_sdc':>7} {'on_sdc':>7} {'off_sdc':>7}")
for b in pmu["native"]:
    ni = pmu["native"][b]["hw_insts"]
    oi = pmu["bar_on"].get(b, {}).get("hw_insts", 0)
    fi_ = pmu["bar_off"].get(b, {}).get("hw_insts", 0)
    nsdc = fi["native"].get(b, {}).get("sdc_pct", float("nan"))
    osdc = fi["bar_on"].get(b, {}).get("sdc_pct", float("nan"))
    fsdc = fi["bar_off"].get(b, {}).get("sdc_pct", float("nan"))
    print(f"{b:22} {ni:>9} {oi:>9} {fi_:>9} {oi/ni:>7.2f} {fi_/ni:>7.2f} "
          f"{nsdc:>7.2f} {osdc:>7.2f} {fsdc:>7.2f}")
print("==== END ====")
print(f"[{TARGET}] 可注错静态点数  on={sum(SITES['bar_on'].values())}  "
      f"off={sum(SITES['bar_off'].values())}  native={sum(SITES['native'].values())}  "
      f"(on~off 说明冗余没被折叠掉，SDC=0 是真防住不是点做没了)")
