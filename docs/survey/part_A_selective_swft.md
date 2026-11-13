## A. 选择性软件容错与 SDC 感知保护（Selective Software Fault Tolerance / SDC-Aware Protection）

Scope note: entries are grouped by role in the project pipeline (full-redundancy baselines → criticality/SDC-driven selection → SDC analysis & prediction infrastructure → control-flow checking → open LLVM/space toolchains → recent 2022–2026 / RISC-V work). Each entry gives first author, venue/year, mechanism, why it matters to this project, code URL if any (verified or marked to-verify), and paper URL/DOI.

### A.1 Full-duplication / full-TMR baselines（"software TMR" 版本的对比基线）

**1. EDDI — Error Detection by Duplicated Instructions in Super-scalar Processors**
- First author: Nahmsuk Oh (with P. Shirvani, E. J. McCluskey), Stanford CRC
- Venue/year: IEEE Transactions on Reliability 51(1), 2002 (~815 citations)
- Mechanism: pure-software duplication of every data-processing instruction into a shadow register/memory space, with comparison instructions inserted before stores and branches; shadow memory uses an address offset so a single upset cannot corrupt both copies.
- Why it matters: the canonical "protect everything" data-flow baseline. The per-instruction criticality score is a selection function over EDDI's candidate set; EDDI = that scoring function saturated to 1.0. Report overhead relative to it.
- Code: none original; faithful LLVM reimplementations exist in ASPIS (entry 41) and COAST (entry 40).
- Paper: https://ieeexplore.ieee.org/abstract/document/994913 (DOI 10.1109/24.994913)

**2. ED4I — Error Detection by Diverse Data and Duplicated Instructions**
- First author: Nahmsuk Oh
- Venue/year: IEEE Transactions on Computers 51(2), 2002
- Mechanism: instead of bit-identical duplicates, the second version computes on data multiplied by a diversity factor k (AN-code style); the comparison is x2 == k*x1. Detects errors that affect both copies identically (common-mode, opcode errors).
- Why it matters: direct intellectual ancestor of "approximate TMR with tolerance delta" — ED4I already breaks the bit-exactness assumption of voting. Cite it so reviewers do not call delta-voting unprecedented; differentiate by arguing delta tolerates *approximation error*, not a known algebraic factor.
- Paper: https://dl.acm.org/doi/10.1109/12.980007

**3. SWIFT — Software Implemented Fault Tolerance**
- First author: George A. Reis (Princeton Liberty group, with D. I. August)
- Venue/year: CGO 2005 (~1100 citations)
- Mechanism: compiler-level instruction duplication that *drops* memory duplication by assuming ECC memory (reclaiming EDDI's biggest cost), moves checks to stores/branches, and folds in enhanced control-flow checking via signatures; exploits idle ILP slots.
- Why it matters: the standard baseline in every comparable paper, and its "assume ECC for memory, protect only the datapath" assumption is exactly the assumption that the runtime ECC-rate monitor makes *dynamic* rather than static. Framing opportunity: SWIFT fixes the assumption at compile time; this project adapts it online.
- Paper: https://liberty.princeton.edu/Publications/cgo05_swift.pdf , https://ieeexplore.ieee.org/document/1402092

**4. SWIFT-R / TRUMP / MASK — Automatic Instruction-Level Software-Only Recovery**
- First author: Jonathan Chang / George A. Reis
- Venue/year: DSN 2006 (pp. 83–92); extended as IEEE Micro 27(1):36–47, 2007 (Top Picks)
- Mechanism: SWIFT-R triplicates compute instructions into three register sets and inserts in-register majority voting at sync points (software TMR with correction, not just detection). TRUMP replaces one replica with an AN-coded (x3) computation so only two streams are needed. MASK enforces program invariants cheaply.
- Why it matters: **closest baseline for "software TMR", and also the strongest threat to the novelty of "approximate TMR"**: TRUMP is already "1 exact + 1 cheaper-but-not-identical replica". The contribution must be framed as (a) *approximation-aware* delta-voting on numeric kernels, plus (b) runtime version switching. Reported: SWIFT-R cuts SDC to ~0.81%, SEGV to ~1.93%.
- Paper: https://liberty.princeton.edu/Publications/dsn06_trump.pdf ; IEEE Micro: https://liberty.princeton.edu/Publications/micro07_trump.pdf

**5. CRAFT + PROFiT — Software-Controlled Fault Tolerance**
- First author: Jonathan Chang (with Reis, August)
- Venue/year: ACM TACO 2(4), 2005
- Mechanism: CRAFT = hybrid HW/SW schemes (checking store buffer, load value queue) that patch SWIFT's unprotected windows; PROFiT uses profiling to choose, per program region, which protection level to apply, trading reliability against performance.
- Why it matters: **PROFiT is the historical origin of "configurable reliability / multi-version code"** — the thing being rebuilt here with modern LLVM plus a runtime ECC-rate signal. Must-cite; the delta over PROFiT is online adaptation plus approximation as a third version.
- Paper: https://dl.acm.org/doi/10.1145/1113841.1113843 (also https://www.academia.edu/9018951/)

**6. nZDC — A Compiler Technique for near Zero Silent Data Corruption**
- First author: Moslem Didehban (ASU MPS Lab, with A. Shrivastava)
- Venue/year: DAC 2016
- Mechanism: duplication at the *machine-code* (post-register-allocation) level that also protects "critical" instructions (loads, stores, branches) by loading written values back and comparing, plus an architecture-specific control-flow check. Closes the SDC holes EDDI/SWIFT leave in memory-access and branch instructions.
- Why it matters: shows that IR-level duplication (what an LLVM-IR pass does) has an intrinsic coverage ceiling because backend register allocation and optimization break redundancy. Must be addressed explicitly — see also entry 46 (compiler-induced reliability loss).
- Code: not released; TUM's COMPAS reimplements nZDC for RISC-V (entry 42).
- Paper: https://dl.acm.org/doi/10.1145/2897937.2898103 ; full detail in thesis: https://mpslab-asu.github.io/publications/papers/Didehban2018THESIS.pdf

**7. NEMESIS — A Software Approach for Computing in Presence of Soft Errors**
- First author: Moslem Didehban
- Venue/year: ICCAD 2017 (pp. 297–304)
- Mechanism: triple-stream (M/R/D) execution with nZDC-style load-back store detectors; on mismatch it runs a diagnosis routine plus majority voting and memory restoration, achieving forward recovery without full checkpoint restart.
- Why it matters: direct competitor to the "software TMR version with recovery"; its diagnosis-on-detection structure is a cheaper alternative to always-on triplication and is worth adding as a *fourth* protection level (detect-then-vote instead of always-vote).
- Paper: https://dl.acm.org/doi/10.5555/3199700.3199740

**8. InCheck — An In-Application Recovery Scheme for Soft Errors**
- First author: Moslem Didehban (with S. R. D. Lokam, A. Shrivastava)
- Venue/year: DAC 2017
- Mechanism: lightweight in-application checkpointing co-designed with nZDC detectors; on detection it rolls back to a fine-grained register/memory checkpoint rather than restarting the task.
- Why it matters: if the framework needs recovery (not only detection) for the "none"/"approximate" versions, InCheck is the cheap complement to TMR and a natural ablation point ("TMR vs detect+rollback at the same observed error rate").
- Paper: https://dl.acm.org/doi/10.1145/3061639.3062272

**9. Generic Soft Error Data and Control Flow Error Detection by Instruction Duplication**
- First author: Moslem Didehban (with Hwisoo So, Prudhvi Gali, A. Shrivastava, Kyoungwoo Lee)
- Venue/year: IEEE TDSC 21(1):78–92, 2023
- Mechanism: makes nZDC's control-flow checking ISA-independent by duplicating compare/branch computation and verifying branch direction generically; implemented as late backend passes in LLVM-or1k so that standard optimizations cannot remove the redundancy.
- Why it matters: the most recent "full duplication, near-complete coverage" reference point, and an explicit treatment of *where in the pass pipeline* to insert redundancy — a decision this project must make for both x86_64 and rv64gcv.
- Paper: https://mpslab-asu.github.io/publications/papers/Didehban2023TDSC.pdf (DOI 10.1109/TDSC.2023.3245842)

### A.2 Selective / criticality-driven protection（本课题核心）

**10. Shoestring — Probabilistic Soft Error Reliability on the Cheap**
- First author: Shuguang Feng (with S. Gupta, A. Ansari, S. Mahlke, University of Michigan)
- Venue/year: ASPLOS 2010 (pp. 385–396)
- Mechanism: relies on symptom-based detection (traps, segfaults) for most errors and uses compiler analysis to find the small set of "high-value, symptom-free" instructions whose corruption would silently reach output (short distance to a store/output, no masking consumer); duplicates only those.
- Why it matters: **the archetype of the distance-to-output heuristic.** Shoestring's safe-vs-vulnerable classification based on consumer analysis and proximity to program output is essentially the v0 of "semantic criticality". Position the weighted model (distance-to-output + loop depth + liveness) as a generalization and quantify what the extra features buy. Results: 33.9% of symptom-escaping errors recovered at ~15.8% overhead.
- Code: none released (an LLVM implementation is described but not published).
- Paper: https://dl.acm.org/doi/10.1145/1735970.1736063 ; LLVM pubs mirror: https://llvm.org/pubs/2010-03-ASPLOS-Shoestring.html

**11. SDCTune — A Model for Predicting the SDC Proneness of an Application for Configurable Protection**
- First author: Qining Lu (UBC, with K. Pattabiraman, M. S. Gupta, J. A. Rivers / IBM)
- Venue/year: CASES 2014
- Mechanism: heuristic plus regression-tree model over static/dynamic instruction features (instruction type, masking operations such as AND-0/SHR, comparison operations, **nested loop depth**, data width, distance to stores) to rank instructions by SDC-proneness, then duplicates the top-ranked set under an overhead budget.
- Why it matters: **the single most directly overlapping prior work on the scoring model.** SDCTune already reports that comparison instructions in inner loops are less SDC-prone than in outer loops — i.e. the loop-depth feature planned here. Treat it as (a) the baseline scoring function to beat and (b) the main novelty threat; differentiation must come from the multi-version + runtime-adaptive layer, not the feature set alone.
- Paper: https://dl.acm.org/doi/10.1145/2656106.2656111 ; model slides: https://blogs.ubc.ca/dependablesystemslab/files/2014/10/SDCTune-cases-presentation.pdf

**12. Configurable Detection of SDC-causing Errors in Programs**
- First author: Qining Lu (with Guanpeng Li, K. Pattabiraman, M. S. Gupta, J. A. Rivers)
- Venue/year: ACM TECS 16(3), Article 88, 2017
- Mechanism: journal extension of SDCTune adding SDCAuto (an automatically learned regression-tree model, no manual heuristics) and a full cost/benefit selection procedure: choose the duplication set that maximizes SDC coverage per unit of performance overhead.
- Why it matters: the formal statement of "selective instruction duplication cost/benefit optimization" and "configurable reliability" that the runtime switcher is the dynamic version of. Borrow its evaluation protocol (coverage-vs-overhead Pareto curves) verbatim — reviewers in this field expect that plot.
- Paper: https://dl.acm.org/doi/10.1145/2930667

**13. IPAS — Intelligent Protection Against Silent Output Corruption in Scientific Applications**
- First author: Ignacio Laguna (LLNL, with M. Schulz, D. F. Richards, J. Calhoun, L. Olson)
- Venue/year: CGO 2016 (pp. 227–238)
- Mechanism: defines *silent output corruption* (SOC) as the only SDC that matters, trains an ML classifier (features collected by LLVM passes, labels from fault injection with a user-provided verification routine) to predict which instructions cause SOC, then duplicates only those via an LLVM pass.
- Why it matters: canonical "application-level acceptability drives protection" paper; its user-supplied quality/verification routine is precisely the semantics needed to justify an *approximate* replica with tolerance delta. Also the cleanest template for an ML-vs-static-heuristic comparison in the evaluation.
- Code: not publicly released as far as could be verified (LLNL internal); workflow fully described in the paper.
- Paper: https://lukeo.cs.illinois.edu/files/2015_LaScRiCaOl_ipas.pdf ; record: https://experts.illinois.edu/en/publications/ipas-intelligent-protection-against-silent-output-corruption-in-s

**14. Low-Cost Prediction-Based Fault Protection Strategy**
- First author: Sunghyun Park (with Shikai Li, Ze Zhang, Scott Mahlke, University of Michigan)
- Venue/year: CGO 2020
- Mechanism: rather than duplicating a computation, it builds a cheap *predictor* of the value a protected instruction should produce and checks the real value against the prediction, falling back to recomputation on mismatch — an always-on protection cheaper than duplication.
- Why it matters: the closest existing idea to "approximate replica + tolerance threshold": a cheap approximate computation used as a checker. **Principal novelty threat to the approximate-TMR contribution** — read carefully and differentiate (they predict/check 1-of-1; this project votes 1 exact + 2 approximate with delta, enabling *correction*, not only detection).
- Paper: https://dl.acm.org/doi/pdf/10.1145/3368826.3377920 ; program: https://2020.cgo.org/accepted-papers

**15. Trace-and-Brace (TAB) — Bespoke Software Countermeasures against Soft Errors**
- First author: Yousun Ko (Yonsei, with Alex Bradbury / lowRISC, B. Burgstaller, R. Mullins / Cambridge)
- Venue/year: LCTES 2022 (co-located with PLDI 2022)
- Mechanism: traces execution to identify intrinsically robust vs. vulnerable code regions, then "braces" only the vulnerable ones with tailored (bespoke) data-flow and control-flow countermeasures; reports 99.82% data-flow error detection.
- Why it matters: recent, RISC-V-flavored (lowRISC/Cambridge authorship), region-granularity selective hardening — i.e. the *basic-block-level* criticality granularity used here. Evidence that BB granularity is publishable, and a baseline for the BB score.
- Paper: https://dl.acm.org/doi/10.1145/3519941.3535070 ; PDF: https://dl.acm.org/doi/pdf/10.1145/3519941.3535070

**16. Evaluating Compiler IR-Level Selective Instruction Duplication (validity study)**
- Venue/year: FTXS workshop at SC 2019
- Mechanism: compares selective duplication driven by LLVM-IR fault models against RTL and binary-level fault injection; concludes IR-based selective duplication is a cost-effective way to hit a target SDC coverage under an overhead budget, but should *not* be used where SDCs must be eliminated entirely (0.25% of RTL faults escape even full IR duplication).
- Why it matters: **the single most important validity citation for this thesis.** It licenses the IR-level approach for "good-enough reliability under a budget" while stating exactly which claim cannot be made. Use it in the threats-to-validity section.
- Paper: https://sc19.supercomputing.org/proceedings/workshops/workshop_files/ws_ftxs105s2-file1.pdf

**17. Deploying Lightweight Input-Aware Selective Instruction Duplication in HPC Applications**
- First author: Md Hasanur Rahman (with Guanpeng Li, University of Iowa / University of Florida)
- Venue/year: SC 2025 (pp. 1099–1112)
- Mechanism: makes selective duplication *input-aware* — a program's SDC profile shifts with input, so the protection set is chosen with lightweight dynamic profiling rather than one static profile, with deployment cost explicitly accounted for.
- Why it matters: 2025 state of the art in exactly this subfield and the most recent competitor. Conceptual parallel worth stating in the abstract: they adapt to *input*, this project adapts to *environment (ECC/soft-error rate)*.
- Paper: https://dl.acm.org/doi/10.1145/3712285.3759822 ; DBLP: https://dblp.org/db/conf/sc/sc2025

**18. ApproxDup — An Approximate Instruction Duplication Mechanism for Efficient SDC Detection in GPGPUs**
- First author: Xiaohui Wei (Jilin University, et al.)
- Venue/year: IEEE TCAD 43(3):1051–…, 2024 (online 2023)
- Mechanism: duplicates SDC-vulnerable instructions with *approximate* duplicates (reduced-precision / simplified replicas) and compares results with a slack, covering 92.51% of SDCs at much lower overhead than exact selective duplication.
- Why it matters: **the strongest direct threat to the "approximate TMR with tolerance delta" claim** — approximate duplication with threshold comparison already exists in a top journal. Cite it and differentiate on (i) TMR/correction vs duplication/detection, (ii) CPU + RISC-V vector targets vs GPGPU, (iii) runtime version switching.
- Code/artifact: Zenodo artifact record https://zenodo.org/records/8220533 (1.4 GB)
- Paper: https://dl.acm.org/doi/abs/10.1109/TCAD.2023.3330821 ; abstract: https://ui.adsabs.harvard.edu/abs/2024ITCAD..43.1051W/abstract

**19. A Study on Selective Implementation Approaches for Soft Error Detection (S-SWIFT-R)**
- First author: Mahsa Nikseresht (KU Leuven, with J. Vankeirsbilck, D. Pissoort et al.)
- Venue/year: Electronics (MDPI) 11(20):3380, 2022; related conference paper "A Selective Soft Error Protection Method for COTS Processor-based Systems" (2021) and a 2025 PhD chapter
- Mechanism: systematically compares *criteria* for choosing which variables/registers to protect with selective SWIFT-R (register liveness, runtime-monitored memory interaction, criticality ranking) on COTS embedded processors.
- Why it matters: an existing explicit study of exactly the criteria weighted here (notably **variable liveness**), on COTS parts in a dependability setting close to space. Use it to justify feature weights instead of asserting them.
- Paper: https://www.mdpi.com/2079-9292/11/20/3380 (open access) ; conference version: https://lirias.kuleuven.be/3643872 ; PDF: https://lirias.kuleuven.be/retrieve/640290 ; 2025 thesis chapter: https://lirias.kuleuven.be/retrieve/3038e39c-b5ff-4c4d-9d1c-6e61adf36c60

### A.3 SDC analysis, vulnerability modeling and prediction（criticality 标签的来源）

**20. Relyzer — Exploiting Application-Level Fault Equivalence to Analyze Application Resiliency to Transient Faults**
- First author: Siva Kumar Sastry Hari (UIUC, with S. V. Adve, H. Naeimi, P. Ramachandran)
- Venue/year: ASPLOS 2012
- Mechanism: prunes the fault-injection space using fault-equivalence classes (control equivalence, store equivalence, def-use pruning), then injects into a small set of representatives to obtain near-exhaustive per-instruction resiliency profiles.
- Why it matters: provides statistically sound per-instruction ground truth without millions of injections — exactly what is needed to validate the criticality score. Methodological ancestor of everything else in this section.
- Paper: https://dl.acm.org/doi/10.1145/2150976.2150990 ; discussed in: http://rsim.cs.illinois.edu/Pubs/16-MICRO-approxilyzer.pdf

**21. Approxilyzer — Towards a Systematic Framework for Instruction-Level Approximate Computing and its Application to Hardware Resiliency**
- First author: Radha Venkatagiri (UIUC, with A. Mahmoud, S. K. S. Hari, S. V. Adve)
- Venue/year: MICRO 2016
- Mechanism: extends Relyzer with an output *quality* metric: for virtually every operand bit of every dynamic instruction it classifies the outcome as masked / detected / output-corrupted, and for corruptions it quantifies quality degradation against an acceptable threshold.
- Why it matters: **the exact formal machinery this project needs** to decide which instructions may run approximately and what delta is acceptable. Approximation potential and resiliency are computed in one framework; the "none / approximate / TMR" three-way decision is an instantiation of it. Must-read.
- Paper: http://rsim.cs.illinois.edu/Pubs/16-MICRO-approxilyzer.pdf

**22. gem5-Approxilyzer — An Open-Source Tool for Application-Level Soft Error Analysis**
- First author: Radha Venkatagiri (with K. Ahmed, A. Mahmoud, S. Misailovic, D. Marinov, C. W. Fletcher, S. V. Adve)
- Venue/year: DSN 2019 (pp. 214–221)
- Mechanism: open-source reimplementation of Relyzer + Approxilyzer on gem5 for x86 (original used proprietary Simics/SPARC), with static and dynamic analysis plus heuristics giving 3–6 orders of magnitude reduction in injection runtime.
- Why it matters: **open source + x86_64 + instruction-level SDC labels** — the most usable ground-truth generator for the x86 target, and extensible to RISC-V inside gem5, which would itself be a defensible engineering contribution of the thesis.
- Code (verified): https://github.com/rsimgroup/gem5-approxilyzer
- Paper: http://rsim.cs.illinois.edu/Pubs/19-DSN-gem5Approxilyzer.pdf

**23. Minotaur — Adapting Software Testing Techniques for Hardware Errors**
- First author: Abdulrahman Mahmoud (UIUC, with Venkatagiri, Ahmed, Misailovic, Marinov, Fletcher, Adve)
- Venue/year: ASPLOS 2019
- Mechanism: treats resiliency analysis as software testing — applies test-suite minimization, prioritization and reduction to error-injection campaigns on top of Approxilyzer; both faster and finds *more* SDC-causing PCs (SDC-PCs).
- Why it matters: if the criticality model is trained or validated by injection, Minotaur is how the campaign becomes affordable in a master's timeline. Also supplies the "SDC-PC" vocabulary reviewers use.
- Code: not separately released (built on the Approxilyzer / gem5 line above).
- Paper: https://ma3mool.github.io/publication/minotaur2019.html ; PDF: https://dijkstra.eecs.umich.edu/kleach/eecs481/sp20/readings/minotaur.pdf

**24. Trident — Modeling Soft-Error Propagation in Programs**
- First author: Guanpeng Li (UBC, with K. Pattabiraman, S. K. S. Hari, M. Sullivan, T. Tsai)
- Venue/year: DSN 2018 (best-paper recognized)
- Mechanism: a three-level analytical model (static instruction-level propagation, control-flow, memory) that predicts per-instruction and whole-program SDC probability *without* fault injection, within a few percent of FI results.
- Why it matters: **the strongest alternative to the static weighted scoring model** — Trident derives propagation probabilities analytically instead of weighting hand-picked features. Either adopt it as the scorer (defensible, strong) or justify why a lighter weighted model suffices. Open source and LLVM-based, so it drops into the toolchain.
- Code (verified): https://github.com/DependableSystemsLab/Trident
- Paper: https://research.nvidia.com/sites/default/files/pubs/2018-06_Modeling-Soft-Error/18-DSN-Li-Trident_0.pdf

**25. vTrident — Modeling Input-Dependent Error Propagation in Programs**
- First author: Guanpeng Li (with K. Pattabiraman)
- Venue/year: DSN 2018 (companion paper)
- Mechanism: simplifies Trident's memory sub-model to *bound* how a program's SDC probability varies across multiple inputs, far faster than per-input FI or full Trident; also identifies the input that yields the median SDC rate.
- Why it matters: runtime switching must be robust across inputs; vTrident is the cheap way to show the criticality ranking is input-stable (or is not). Ships in the Trident repository under test/vTrident.
- Code (verified, same repo): https://github.com/DependableSystemsLab/Trident
- Paper: https://dl.acm.org/doi/10.1109/DSN.2018.00028 (see repo README for both paper links)

**26. GPU-Trident — Efficient Modeling of Error Propagation in GPU Programs**
- First author: Abdul Rehman Anwer (UBC, with M. Sullivan, G. Li, T. Tsai, K. Pattabiraman, S. K. S. Hari)
- Venue/year: SC 2020 (preliminary version SELSE 2019)
- Mechanism: ports Trident to GPU kernels, handling thread-level parallelism and shared-memory effects; predicted 35.67% vs FI-measured 33.73% mean SDC probability, instruction-level Pearson correlation ~0.83.
- Why it matters: shows how an analytical propagation model must be re-derived when the execution model changes — directly instructive for extending the scorer to **RVV vector instructions** (lanes, masking, strip-mining), an open gap this thesis can claim.
- Paper: https://www.mbsullivan.info/attachments/papers/anwer2020gpu.pdf ; SELSE version: https://d1qx31qr3h6wln.cloudfront.net/publications/SELSE2019_GPUTrident.pdf

**27. ePVF — An Enhanced Program Vulnerability Factor Methodology for Cross-Layer Resilience Analysis**
- First author: Bo Fang (UBC, with Q. Lu, K. Pattabiraman, M. Ripeanu, S. Gurumurthi)
- Venue/year: DSN 2016 (pp. 168–179)
- Mechanism: refines ACE/PVF analysis by filtering *crash-causing* bits out of the vulnerable set so the remainder approximates true SDC vulnerability; requires a detailed dynamic data-dependence graph, which limits it to roughly a million dynamic instructions.
- Why it matters: the pre-Trident analytical baseline, useful for the related-work taxonomy (ACE/PVF → ePVF → Trident → ML/GNN) and a reminder that crash is not SDC — a distinction the criticality score must encode, since in a watchdog-protected spacecraft a crash is often the *benign* outcome.
- Paper: https://store.computer.org/csdl/pds/api/csdl/proceedings/download-article/12OmNAY79gf/pdf (DOI 10.1109/DSN.2016.24)

**28. Hamartia — A Fast and Accurate Error Injection Framework**
- Venue/year: DSN-W (workshop) 2018 (NVIDIA / UBC collaboration)
- Mechanism: uses RTL gate-level fault injection to show that the ubiquitous single-bit-flip model misses many realistic hardware faults (single-event multi-bit / multi-location effects), and provides a faster injector supporting richer error models.
- Why it matters: **threat-model hygiene.** If switching decisions are tuned under a single-bit-flip assumption, this is the citation reviewers will use against the work. State the fault model explicitly, and note that delta-threshold voting is arguably *more* robust to multi-bit corruption than exact comparison.
- Paper: https://research.nvidia.com/publication/2018-06_hamartia-fast-and-accurate-error-injection-framework ; PDF: https://d1qx31qr3h6wln.cloudfront.net/publications/dsn-w_Hamartia_Framework.pdf

**29. GATPS — An Attention-Based Graph Neural Network for Predicting SDC-Causing Instructions**
- First author: J. Ma (et al.)
- Venue/year: 2021 (IEEE)
- Mechanism: predicts SDC-causing instructions by learning an embedding for each instruction through attention-based aggregation over the program graph.
- Why it matters: the first clear GNN instance of the criticality scorer. If a hand-weighted linear model is presented in 2026, a reviewer will ask why not a GNN; have the answer ready (interpretability, compile-time cost, no need for FI labels).
- Paper: https://ieeexplore.ieee.org/iel7/9440994/9440995/09441056.pdf

**30. Efficient Instruction Vulnerability Prediction With Heterogeneous SDC Propagation Knowledge Graph**
- First author: Bao Wen (NUAA, with Jingjing Gu, Dazhong Shen, Qiang Zhou, Fuzhen Zhuang et al.)
- Venue/year: IEEE TDSC 23(1):1173–1190, Jan–Feb 2026 (online 2025), DOI 10.1109/TDSC.2025.3614343
- Mechanism: builds a multi-layer heterogeneous program knowledge graph of instruction entities and relations that encodes plausible error-propagation paths, combines a GNN with a Transformer to model intra- and inter-basic-block context, and predicts vulnerable instructions at low overhead; trained on 1,144,070 LLFI fault injections across 22 programs.
- Why it matters: the current SOTA accuracy bar for SDC prediction, from a Chinese institution the committee will likely know. Its intra/inter-basic-block modeling competes directly with the BB-level score here, and it is a ready-made source of feature definitions.
- Paper: https://www.computer.org/csdl/journal/tq/2026/01/11179941/2aiMtZyBi0w ; IEEE: https://ieeexplore.ieee.org/document/11179941

**31. GVR — A Unified Graph Neural Framework for Instruction-Level Soft Error Vulnerability Prediction and Ranking**
- First author: Mohammad Reza Pirayesh (with Mohsen Raji et al., Shiraz University)
- Venue/year: Journal of Systems Architecture, vol. 181, 2026 (in press)
- Mechanism: a single graph representation fusing static program graphs with dynamic execution information and enriched instruction features, used both to *predict* vulnerability and to *rank* instructions for selective protection.
- Why it matters: "predict + rank for selection" is exactly pipeline stage 1 → stage 2 of this project; the newest entry in the field, so cite it to demonstrate currency.
- Paper: https://www.sciencedirect.com/science/article/pii/S138376212600353X

**32. LLFI / LLTFI — LLVM-level fault injection**
- First author: UBC DependableSystemsLab (LLFI: Thomas & Pattabiraman; LLTFI extends to ONNX/TensorFlow/PyTorch)
- Venue/year: LLFI ~2013 (DSN-W / ISSRE line); LLTFI 2022
- Mechanism: injects faults into LLVM IR at selected program points with traceability back to source code; LLTFI adds ML-model injection through onnx-mlir.
- Why it matters: **the most likely experimental harness for this thesis**, and the de facto standard against which numbers will be compared (the TDSC 2026 work above used LLFI for >1.1M injections). Keep the IR-vs-RTL fidelity caveat from entry 16 in mind.
- Code (verified): https://github.com/DependableSystemsLab/LLFI , https://github.com/DependableSystemsLab/LLTFI , GPU variant https://github.com/DependableSystemsLab/LLFI-GPU , invariant-based variant https://github.com/DependableSystemsLab/LLFI-IPA
- Paper: see PaperLLFI.bib in the repository

**33. ZOFI — Zero-Overhead Fault Injection Tool for Fast Transient Fault Coverage Analysis**
- First author: Vasileios Porpodas (Intel; previously Cambridge, DRIFT co-author)
- Venue/year: arXiv 1906.09390 (2019), tool paper
- Mechanism: injects transient faults into an unmodified *binary* at runtime with negligible overhead (ptrace-style register corruption), enabling fast coverage campaigns on real hardware.
- Why it matters: allows evaluation on real x86_64 silicon with no simulator, complementing LLFI; cheap enough for the many configurations a multi-version framework generates.
- Code: https://github.com/vporpo/zofi (repo named in the paper; to-verify)
- Paper: https://arxiv.org/html/1906.09390v4

### A.4 Control-flow checking（CFG 级对应机制，空天场景性价比很高）

**34. CFCSS — Control-Flow Checking by Software Signatures**
- First author: Nahmsuk Oh (with P. Shirvani, E. J. McCluskey)
- Venue/year: IEEE Transactions on Reliability 51(1):111–122, 2002
- Mechanism: assigns a compile-time signature to each basic block; a runtime signature register is XOR-updated on each edge and checked on block entry, detecting illegal inter-block jumps. Branch-fan-in nodes need an adjusting signature.
- Why it matters: the mandatory CFC baseline, already implemented in two open toolchains that can be reused (COAST and ASPIS), so it comes nearly free in the multi-version matrix.
- Implementations: COAST documentation with algorithm and modifications: https://coast-compiler.readthedocs.io/en/latest/cfcss.html ; ASPIS: https://github.com/HEAPLab/ASPIS
- Paper: https://ieeexplore.ieee.org/document/994926 ; summary: https://www.semanticscholar.org/paper/eca217cfa0c878c9685235ab1ca39ead34946b63

**35. CEDA — Control-Flow Error Detection Using Assertions**
- First author: Ramtilak Vemu (with Jacob A. Abraham, UT Austin)
- Venue/year: IEEE Transactions on Computers 60(9):1233–1245, 2011
- Mechanism: splits signature updates into two parts per basic block (entry and exit) with values chosen so that both inter-block and many intra-block control-flow errors are caught, at lower overhead and higher coverage than CFCSS.
- Why it matters: the standard "better than CFCSS" comparison point; implementing only CFCSS invites the CEDA/RASM question at defense.
- Paper: https://dl.acm.org/doi/10.1109/TC.2011.101 ; IEEE: https://ieeexplore.ieee.org/document/5871589

**36. RASM — Random Additive Signature Monitoring for Control Flow Error Detection**
- First author: Jens Vankeirsbilck (KU Leuven, with N. Penneman, H. Hallez, J. Boydens)
- Venue/year: IEEE Transactions on Reliability 66(4):1178–1192, 2017
- Mechanism: additive (not XOR) signature updates using *random* values with optimally placed validity checks — two gradual updates and one verification per basic block, so skipping any update makes the runtime signature unrecoverable. Higher detection ratio than the CFCSS/CEDA family.
- Why it matters: the modern CFC of choice, already implemented in ASPIS and in TUM's RISC-V backend — i.e. the CFC to ship in the framework's high-protection level.
- Paper: https://ieeexplore.ieee.org/iel7/24/8124127/08067656.pdf ; record: https://ui.adsabs.harvard.edu/abs/2017ITR....66.1178V/abstract

**37. RACFED — Random Additive Control Flow Error Detection**
- First author: Jens Vankeirsbilck (with N. Penneman, H. Hallez, J. Boydens)
- Venue/year: SAFECOMP 2018 (LNCS, pp. 220–234)
- Mechanism: extends RASM with intra-block instruction monitoring so that both inter-block and *intra*-block CFEs are detected by one mechanism, avoiding the cost of stacking two techniques (e.g. RASM + RSCFC/SIED).
- Why it matters: if the criticality model distinguishes intra-BB from inter-BB control errors, RACFED is the technique to borrow; it also covers the SIED line of work through its comparisons.
- Paper: https://lirias.kuleuven.be/retrieve/c61d5f98-a5a6-4a5b-9743-5b9fd3fd0348

**38. PaSS — Path Sensitive Signatures for Control Flow Error Detection**
- First author: Ze Zhang (University of Michigan, with Sunghyun Park, Scott Mahlke)
- Venue/year: LCTES 2020
- Mechanism: partitions the CFG into intervals, continuously updates a path-memorizing signature, and uses a single check per interval to catch both intra-interval (short-distance) and inter-interval (long-distance) CFEs; adds low-cost inter-procedural protection. 99.0% fault coverage at 19.1% overhead.
- Why it matters: the best overhead/coverage point in the recent compiler CFC literature and a *selective* (interval-granularity) scheme — the control-flow analogue of BB-level criticality. Strong candidate to adopt rather than reinvent.
- Paper: https://cccp.eecs.umich.edu/papers/ze-lctes20.pdf ; ACM: https://dl.acm.org/doi/abs/10.1145/3372799.3394360

**39. DRIFT — Decoupled Compiler-Based Instruction-Level Fault Tolerance**
- First author: Konstantina Mitropoulou (Cambridge, with V. Porpodas, T. M. Jones)
- Venue/year: LCPC 2013 (Springer LNCS; frequently cited as the 2014 chapter)
- Mechanism: observes that checking code in EDDI/SWIFT-style duplication serializes against control flow; DRIFT *decouples* redundant data-flow from control-flow checks, batching and delaying checks to recover ILP — up to ~29% faster than the state of the art at equal coverage.
- Why it matters: a pure *performance* optimization applicable to all three code versions, most valuable in the triplicated version. See also the companion COMET (CASES 2016) on communication-optimized multithreaded detection.
- Paper: https://link.springer.com/chapter/10.1007/978-3-319-09967-5_2 ; chapter record: https://www.researchgate.net/publication/303542655 (cited within the nZDC references)

### A.5 开源 LLVM 容错工具链与空天应用（实现参考与基线）

**40. COAST — COmpiler-Assisted Software fault Tolerance (BYU CCL)**
- First author (tool paper): Matthew Bohman / Benjamin James (with M. Wirthlin, H. Quinn, J. Goeders)
- Venue/year: "Microcontroller Compiler-Assisted Software Fault Tolerance", IEEE Transactions on Nuclear Science 66(1):223–232, 2019; follow-ups on multiple processor platforms (2019) and FreeRTOS (2021)
- Mechanism: open LLVM IR passes implementing DWC (duplication) and **TMR with inserted voters**, with a configurable *scope of replication* (per-function and per-variable annotations, pointer-crossing handling) plus CFCSS; validated by fault injection (QEMU) **and neutron-beam radiation testing on a TI MSP430**, and demonstrated on ARM and RISC-V.
- Why it matters: **the most important engineering baseline for this thesis.** It is open source, LLVM-based, TMR-capable, space-motivated, multi-ISA (including RISC-V) and radiation-validated. The contribution must be expressible as a clean delta over COAST: per-instruction criticality scoring, approximate replicas with delta-voting, and runtime multi-version switching (COAST's selection is manual annotation and static).
- Code (verified): https://github.com/byuccl/coast ; docs: https://coast-compiler.readthedocs.io
- Paper: https://space.pitt.edu/sites/default/files/2024-10/Microcontroller-Compiler-Assisted-Software-Fault-Tolerance.pdf ; multi-platform: https://ccl.byu.edu/assets/Applying_Compiler-Automated_Software_Fault_Tolerance_to_Multiple_Processor_Platforms_2019.pdf ; RTOS: https://byuccl.github.io/assets/Automated_Software_Compiler_Techniques_to_Provide_Fault_Tolerance_for_Real-Time_Operating_Systems_2021.pdf

**41. ASPIS — Automatic Software-based Protection and Integrity Subsystem**
- First author: Davide Baroffio (Politecnico di Milano HEAPLab, with F. Reghenzani, W. Fornaciari)
- Venue/year: ACM TODAES 29(5), September 2024 ("Enhanced Compiler Technology for Software-based Hardware Fault Detection"); earlier ACM CF 2023 paper
- Mechanism: an LLVM-IR pass suite implementing EDDI for data flow plus CFCSS and RASM for control flow, with overhead-reduction techniques including **use-def-chain-driven selective duplication** that exploits SSA form; language- and architecture-independent by construction.
- Why it matters: the most modern, actively maintained open LLVM SIHFT framework, and its use-def-chain selective duplication is essentially a liveness-based criticality heuristic — one of the three planned features, already published. Build on it rather than from scratch, and benchmark against it.
- Code (verified): https://github.com/HEAPLab/ASPIS ; project page: https://heaplab.github.io/ASPIS
- Paper: https://dl.acm.org/doi/10.1145/3660524

**42. COMPAS — Compiler-Assisted Software-Implemented Hardware Fault Tolerance for RISC-V**
- First author: Uzair Sharif (TU Munich, with D. Mueller-Gritschneder, U. Schlichtmann)
- Venue/year: MECO 2022 (11th Mediterranean Conference on Embedded Computing); companion talk "Extending the RISC-V LLVM Backend to Support Fault-tolerant Computing"
- Mechanism: open-source LLVM **backend** (late, post-optimization) passes for RV32/RV64 implementing EDDI, SWIFT, nZDC, CFCSS, RASM and their own REPAIR scheme, with support for **selective hardening of critical functions**.
- Why it matters: **the RISC-V baseline, and the work that overlaps this project's stated scope most directly** (rv64, LLVM, selective hardening, multiple schemes). Read before finalizing the novelty claim; the remaining differentiators are criticality *scoring* (vs function-level manual selection), approximate TMR, and runtime adaptation. Its backend placement also answers the "optimizations delete my redundancy" problem.
- Code (verified): https://github.com/tum-ei-eda/compas-ft-riscv
- Paper: https://ieeexplore.ieee.org/document/9797144 ; talk abstract: https://www.edacentrum.de/en/node/1660

**43. CompaSeC — A Compiler-assisted Security Countermeasure to Address Instruction Skip Fault Attacks on RISC-V**
- First author: Johannes Geier (TU Munich, with D. Mueller-Gritschneder, U. Sharif, U. Schlichtmann)
- Venue/year: ASP-DAC 2023
- Mechanism: reuses the COMPAS SIHFT passes (duplication plus signature monitoring) as a countermeasure against *instruction-skip* fault-injection attacks on RISC-V, combining selective data- and control-flow hardening.
- Why it matters: shows that one compiler infrastructure serves both radiation and attack threat models; the natural place to discuss RISC-V CFI / soft-error variants if the thesis wants a security-adjacent paragraph.
- Paper: https://www.aspdac.com/aspdac2023/archive/pdf/8C-3.pdf ; ACM: https://dl.acm.org/doi/10.1145/3566097.3567925

**44. REDDI — Hardening On-Board Software: a Low-Overhead Compiler-based Approach**
- First author: Emilio Corigliano (Politecnico di Milano, with HEAPLab and Argotec)
- Venue/year: EDHPC 2025 (2nd European Data Handling & Data Processing Conference, ESA; IEEE Xplore document 11325983)
- Mechanism: a *selective* SIHFT technique applied at compilation time, targeting satellite on-board software with substantially reduced overhead compared with full EDDI-style duplication; part of the ESA-funded ASTRAEUS line and built in the ASPIS ecosystem.
- Why it matters: **the closest paper in the world to this thesis topic in 2025** — compiler-based selective hardening explicitly for spacecraft on-board software, ESA-funded. Read it first: it shows what the European space community considers novel and which overhead numbers are credible.
- Paper (open PDF): https://re.public.polimi.it/bitstream/11311/1298225/1/2025_EDHPC_REDDI.pdf ; IEEE: https://ieeexplore.ieee.org/document/11325983 ; session: https://indico.esa.int/event/552/contributions/11091

**45. ASTRAEUS — Software Techniques for Soft Error Resilience: the ASTRAEUS Project**
- First author: Federico Reghenzani (PoliMi, with D. Baroffio, E. Corigliano, W. Fornaciari, G. S. Gajani, P. Maffezzoni, plus Argotec)
- Venue/year: DSD 2025 (28th Euromicro Conference on Digital System Design, pp. 138–144); ESA Contract 4000147102/25/NL/RA, ARTES 4.0
- Mechanism: project overview of SIHFT-based radiation mitigation for COTS devices in space telecom payloads — modifying a conventional compilation toolchain to identify transient faults and apply fault-tolerant responses, validated by fault injection and radiation testing.
- Why it matters: supplies the authoritative framing paragraph for "COTS in space + compiler-based mitigation", including the validation standards (ESA/ECSS radiation-effects mitigation handbook) a 空天计算 thesis should cite.
- Paper (open PDF): https://re.public.polimi.it/bitstream/11311/1298452/1/2025___DSD___Astraeus.pdf ; record: https://re.public.polimi.it/handle/11311/1298452

**46. Quantifying Compiler-Induced Reliability Loss in Software-Based Fault Tolerance**
- First author: Davide Baroffio (with J. Geier et al.; PoliMi / TUM)
- Venue/year: ASP-DAC 2026 (preprint on mediaTUM)
- Mechanism: measures how standard LLVM optimization passes *erode* redundancy inserted at IR level (CSE, DCE, GVN removing duplicated computation), quantifying the resulting coverage loss for ASPIS/COAST-style tools.
- Why it matters: **a must-cite implementation hazard.** It decides whether to insert redundancy at IR level (portable but optimizable-away) or at backend level (robust but per-ISA work doubled for x86_64 and RV64). Expect this question at the defense.
- Paper (preprint): https://mediatum.ub.tum.de/doc/1836695/pp6qy0y8uhnyf7z0srwwm4m2g.baroffiogeier_aspdac26_preprint.pdf

**47. Characterizing Runtime Performance Variation in Error-Detecting (EDDI-hardened) Programs**
- Venue/year: ISSRE 2023
- Mechanism: an empirical study of why EDDI-protected binaries exhibit large, hard-to-predict runtime variation (ILP, execution-port pressure, cache effects) instead of the textbook ~2x slowdown.
- Why it matters: the runtime switcher's cost model needs *predictable* per-version overheads; this paper shows they are not predictable and demonstrates how to measure honestly. Also a WCET concern for real-time space workloads.
- Paper: https://store.computer.org/csdl/proceedings-article/issre/2023/159400a730/1RKjmKdNTna

**48. Software Fault Tolerance in Real-Time Systems: Identifying the Future Research Questions**
- First author: Federico Reghenzani (with Zhishan Guo)
- Venue/year: ACM Computing Surveys 55(14s), July 2023 (DOI 10.1145/3589950)
- Mechanism: surveys SIHFT in real-time contexts and proposes a joint scheduling/failure analysis model, showing how redundancy interacts with timing guarantees.
- Why it matters: switching versions at runtime creates a schedulability problem (each version has a different WCET). This survey is both the related-work anchor and the source of the mixed-criticality vocabulary a 空天 thesis committee expects.
- Paper: https://zguo32.wordpress.ncsu.edu/files/2023/04/Reghenzani-and-Guo-2023-Software-Fault-Tolerance-in-Real-Time-Systems-Ide.pdf

**49. EXPERT — Effective and Flexible Error Protection by Redundant Multithreading**
- First author: Hwisoo So (Yonsei, with M. Didehban, Y. Ko, A. Shrivastava, K. Lee)
- Venue/year: DATE 2018
- Mechanism: redundant multithreading in which the redundancy level is *flexible*, trading coverage against thread resources instead of enforcing lock-step full RMT.
- Why it matters: the multicore alternative to intra-thread TMR. If the target platform has spare cores, reviewers will ask why instructions are triplicated rather than threads; have the comparison ready (RMT needs cores and inter-thread communication; instruction TMR survives on a single hardened core).
- Paper: https://ieeexplore.ieee.org/document/8342067

**50. Duplication-Based Fault Tolerance for RISC-V Embedded Software**
- Venue/year: 2024 (venue not fully verified; indexed on ResearchGate, cites COMPAS)
- Mechanism: evaluates duplication-based SIHFT variants specifically on RISC-V embedded software and quantifies the coverage/overhead trade-off on real cores.
- Why it matters: a recent concrete RISC-V coverage/overhead dataset to compare rv64gcv numbers against. **Verify the venue on DBLP before citing.**
- Link: https://www.researchgate.net/publication/383768042_Duplication-Based_Fault_Tolerance_for_RISC-V_Embedded_Software

**51. Using RISC-V Extensions to Support Software Fault Tolerance (Xpdetector / P-DETECTOR)**
- First author: De Blaere (with J. Vankeirsbilck et al., KU Leuven)
- Venue/year: recent (2023–2025; Semantic Scholar record)
- Mechanism: proposes a proof-of-concept RISC-V extension (Xpdetector) with custom instructions and hardware support that implement the P-DETECTOR SIHFT scheme, cutting the software overhead of duplication-based detection.
- Why it matters: the HW/SW co-design escape hatch for duplication overhead on RISC-V; useful for the future-work section and to pre-empt "why not add an instruction?".
- Link: https://www.semanticscholar.org/paper/c00c1cacc0371aaa834e975c6e65a0624cf6e52d

**52. Reliability and Performance Evaluation of a RISC-V Vector Extension Unit for Vector Multiplication**
- First author: C. Imianosky (with D. A. Santos, D. R. Melo, F. Viel, L. Dilillo)
- Venue/year: IEEE DFT 2024
- Mechanism: RTL fault-injection campaigns on the vector execution unit of HARV-SoC, comparing scalar and RVV-intrinsic implementations for both speedup (up to 28.69x) and reliability across vector parameter configurations.
- Why it matters: **the empirical basis for extending the criticality model to RVV.** Vector width / LMUL choices change the vulnerability profile, so one vector instruction is not "one instruction" for criticality purposes. See also RISC-V Summit Europe 2025 posters describing the first full RTL instruction-replication implementation in an HPC-class vector processor (Vitruvius+): ~75% reduction in non-silent failures at ~7.5% area overhead.
- Paper: https://hal.science/hal-04715405v1/file/2024___DTFS___Reliability_and_Performance_Evaluation_of_a_RISC_V_Vector_Extension_Unit_for_Vector_Multiplication%20%281%29.pdf ; posters: https://riscv-europe.org/summit/2025/posters

**53. Approximate TMR (ATMR) voters and reduced-precision redundancy**
- Representative paper: "A Fault Tolerant Voter for Approximate Triple Modular Redundancy", Electronics (MDPI) 8(3):332, 2019; see also X-Rel (Vafaei et al., 2023) for precision-relaxed voters, reduced-precision redundancy (RPR) work by Junsangsri et al. (2022/2025), and the 2026 TMR survey
- Mechanism: ATMR's defining correctness rule is that *only one* approximate module may differ from the exact one for any given input vector, so a majority of two always exists; the paper designs a voter tolerant to faults at its own inputs and introduces a Quality-of-Circuit metric. X-Rel relaxes voter precision to compare only the most significant bits.
- Why it matters: **hardware ATMR has already formalized the correctness condition the software approximate-TMR scheme needs.** delta-threshold voting is sound only if each replica's approximation error is bounded such that two of three always agree within delta — borrow the ATMR condition and state it as an explicit constraint. X-Rel's precision-relaxed voting is the hardware analogue of delta.
- Paper: https://www.mdpi.com/2079-9292/8/3/332 ; TMR survey: https://arxiv.org/html/2603.14411v1 ; RPR: https://www.techrxiv.org/doi/pdf/10.36227/techrxiv.21350274

**54. Processor Design for Soft Errors: Challenges and State of the Art (survey)**
- Venue/year: ACM Computing Surveys 49(3), Article 57, November 2016
- Mechanism: broad survey of hardware and compiler-based soft-error techniques, including the SWIFT-R / TRUMP recovery line and their cost structures.
- Why it matters: the taxonomy backbone for the related-work chapter, and a source of accepted overhead figures for each classical scheme without re-deriving them.
- Paper: https://li3tuo4.github.io/pub/li-csur.pdf

**55. SDC-causing Error Detection Based on Lightweight Vulnerability Prediction (and the ML-SDC lineage)**
- First author: Yunfei Liu (et al.)
- Venue/year: ACML 2019 (PMLR vol. 101)
- Mechanism: lightweight feature-based ML prediction of per-instruction SDC vulnerability without exhaustive injection, using masking operations, transfer/branch features and loop information; its related-work section maps the whole ML-for-SDC field (SVM: Laguna 2016, Yang & Wang 2019; regression trees: Lu 2014/2017; NN/LSTM: Bronevetsky 2009, Liu 2018 "Instruction SDC Vulnerability Prediction Using LSTM"; random forests: Liu 2019 KSII).
- Why it matters: cite this to cover the entire ML-prediction cluster economically, and to source the exact feature list (mask instructions, loop depth, transfer/branch) that the weighted model should at minimum include.
- Paper (open): https://proceedings.mlr.press/v101/liu19c/liu19c.pdf

### A.6 未能完全核实的条目（引用前务必在 DBLP 核对）

- **"PARIS" / "SDCPredictor"** as standalone named tools: the ML-SDC-prediction cluster was found (entries 29, 30, 31, 55) but no paper titled *PARIS* for SDC prediction at PRDC/DSN could be verified. Laguna's SVM-based work in this space is IPAS (entry 13). Drop the names or verify directly on DBLP.
- **SIED**: consistently referenced as an intra-block CFE technique in the RASM/RACFED papers, but the original publication record was not retrieved. Cite it through Vankeirsbilck's comparisons until verified.
- **"ASTERISC"**: the name collides with (a) AsteRISC, a flexible multi-cycle RISC-V core for design-space exploration (https://asterisc.readthedocs.io) and (b) ethereum-optimism/asterisc, a RISC-V fault-*proof* VM (https://github.com/ethereum-optimism/asterisc). Neither is an SIHFT compiler. A reference to "ASTERISC" for software fault tolerance is most likely a garbled reference to **ASPIS** or **ASTRAEUS** (entries 41, 45).
- **"FT-LLVM"**: no distinct project found. The real open LLVM duplication/TMR passes are COAST, ASPIS and compas-ft-riscv (entries 40–42); note that `compas-ft-riscv` is literally the "ft" LLVM RISC-V repository.
- **Code availability summary**: Minotaur, Shoestring, IPAS, SDCTune and nZDC have no public code. The open-code set in this topic is COAST, ASPIS, compas-ft-riscv, Trident/vTrident, gem5-approxilyzer, LLFI/LLTFI/LLFI-GPU, ZOFI (to-verify) and the ApproxDup Zenodo artifact.

### A.7 Must-read top 10（针对本课题排序）

1. **REDDI (EDHPC 2025)** — selective compiler hardening for spacecraft on-board software; the nearest living neighbor to this thesis. Read first.
2. **COAST (TNS 2019 + GitHub)** — open LLVM DWC/TMR for space, multi-ISA, radiation-validated. The implementation baseline and the yardstick for what remains to contribute.
3. **COMPAS (MECO 2022 + tum-ei-eda/compas-ft-riscv)** — open LLVM *backend* SIHFT for RV32/RV64 with selective hardening of critical functions. The RISC-V baseline and the largest novelty-overlap risk.
4. **SDCTune / Configurable Detection of SDC-causing Errors (CASES 2014 / TECS 2017)** — the published version of criticality scoring plus cost/benefit selection (loop depth, masking, distance to stores). Must be beaten or clearly differentiated.
5. **Approxilyzer (MICRO 2016)** + **gem5-Approxilyzer (DSN 2019, open source)** — the only framework unifying instruction-level approximability and resiliency under an output-quality threshold. The formal justification for delta and for the approximate version.
6. **SWIFT-R / TRUMP (DSN 2006 / IEEE Micro 2007)** — software TMR with in-register majority voting plus AN-coded cheap replicas. Mandatory baseline and the closest prior art to "1 exact + non-identical replicas".
7. **Trident (DSN 2018, open source)** — analytical per-instruction SDC probability without fault injection. Either the scorer or the scorer's strongest competitor; either way it belongs in the experiments.
8. **Shoestring (ASPLOS 2010)** — the origin of "protect only instructions close to output that symptoms will not catch". Ancestor of the distance-to-output feature; cite prominently and generalize honestly.
9. **ApproxDup (TCAD 2024)** + **Low-Cost Prediction-Based Fault Protection (CGO 2020)** — the two papers that already pair a cheap/approximate replica with a slack comparison. Read together; the approximate-TMR novelty statement must survive both.
10. **ASPIS (TODAES 2024, open source)** + **Quantifying Compiler-Induced Reliability Loss (ASP-DAC 2026)** — the modern open IR-level toolchain plus the proof that IR-level redundancy leaks coverage through the optimizer. Settle IR-vs-backend insertion before writing pass code.

Honorable mentions to skim rather than study: PaSS (LCTES 2020) for the cheapest effective CFC; FTXS@SC'19 for the validity limits of IR-level selection; Reghenzani & Guo (CSUR 2023) for the real-time/schedulability consequences of switching versions at runtime; the ATMR voter line (Electronics 2019 / X-Rel 2023) for the correctness condition a delta-voter must satisfy.
