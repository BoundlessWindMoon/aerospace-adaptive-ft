# addr 注入点修正与三层发现（2026-11-20）

## 改动
FI 的 addr 注入从"替换 GEP 的 index 操作数"改为"在下标**定义处**注入并替换其全部使用"。
旧实现把故障注在下标保护（addr-dmr 比较 / addr-correct median）的**下游**，任何下标冗余
结构上都看不到故障 → addr 保护无法被公平评估。新实现 hook 紧跟 def 插入，GEP/比较/median
都看到被扰动值，上游重算的影子保持干净，dominance 安全，IR verify 通过。

## 三层发现（addr 类，SDC/DUE/det_rec%）
- **方法学**：def-injection 后检测开始 fire（旧口径 det≈0）。**所有旧 addr 结果作废，须用新口径重核。**
- **PolyBench 上 addr-dmr + detect+retry 真有收益**：pb_gemm addr DUE 73.8→0（det 15→89），
  pb_jacobi2d DUE 14→0。addr 保护第一次在公平注入下显出真效果。
- **OBPMark 暴露更深的洞**：image_prep/proc 的 47%/25% SDC 未被检测（det 只覆盖一半），
  retry 够不着。根因 `atftBuildIntShadow` 在 2D/指针下标（f_offset/f_scrub 的指针算术）上
  重建不出影子（`s==idx` 即 skip），而简单下标能重建。**addr-dmr 的影子重建覆盖面是真边界。**

## 连带修正
- addr 的正解是 **detect(addr-dmr) + retry**，不是 correct-before-use——后者替换 GEP 操作数，
  使注入点又落回 median 下游，救不了。修正 task #40 "addr-correct" 的定位。
- 旧的"addr-dmr 收益 cast→0/1.91×"是在 P0-4 前的空注入 + 下游注入口径上测的，双重失效，须重核。
