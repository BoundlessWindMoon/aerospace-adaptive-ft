# 半环与阈值：两个理论工具

## 一、min-plus 的精确验证与 APSP 等难（这是一条下界）

| 工作 | 出处 | 结论 |
|---|---|---|
| Williams & Williams, *Subcubic Equivalences Between Path, Matrix, and Triangle Problems* | STOC 2010 / JACM 2018 | **"验证 (min,+) 矩阵乘的正确性"与 APSP 本身次立方等价**：这一组问题要么都有真次立方算法，要么都没有 |
| Mohri, *Semiring Frameworks and Algorithms for Shortest-Distance Problems* | J. Automata, Languages and Combinatorics, 2002 | 热带半环 (R⁺∪{∞}, min, +, ∞, 0) 的代数框架；min 无加法逆元 |
| *Verifying Shortest Paths in Linear Time* | arXiv 2412.06121, 2024 | **单源**最短路有线性时间的认证算法 |

**读数**（对 floyd-warshall 的 16.9% 工作量）：

- 我们之前只说"半环没有加法逆元所以校验和无定义"，这是对的但太浅。
  更强的结论是：**在 (min,+) 上做精确校验，与重算一遍同难**。所以
  "ABFT 式的降阶精确校验"在 min-plus 上**不存在**（在当前复杂性假设下），
  这是一个可引用的下界，不是我们没想到。
- 但**验证**不等于**检测**。单源情形有线性时间认证算法，说明"带见证的检查"
  比"盲验证"便宜得多。可用的缝隙是**单边检测**：
  - 距离被改**小** → 不再对应任何真实路径，可用前驱见证 O(1) 核对；
  - 距离被改**大** → 违反某个三角不等式实例 `d[i][j] ≤ d[i][k] + d[k][j]`。
  随机抽 m 个三元组检查，成本 O(m)，漏检率可算。**这是概率检测而不是验证，
  所以绕得过上面那个下界。**

## 二、阈值的概率界：worst-case 太保守

| 工作 | 出处 | 结论 |
|---|---|---|
| Higham & Mary, *A New Approach to Probabilistic Rounding Error Analysis* | **SIAM J. Sci. Comput. 41(5), 2019**（被引 189+） | 传统 `nu` 界可换成 `λ√n·u`，失败概率 `2exp(−λ²/2)`；给出 `γ̃ₙ(λ)=exp(λ√n u + nu²/(1−u))−1` |
| Connolly, Higham & Mary / Stochastic rounding | SIAM 2021 | 随机舍入下 `√n u` 界无条件成立 |
| *Probabilistic Rounding Error Analysis From A Statistical Perspective* | arXiv 2405.07537, 2024 | 进一步给期望与方差的闭式，比概率界更紧 |
| V-ABFT | arXiv 2602.08043, 2026 | 方差自适应阈值；指出 A-ABFT 的阈值比实际误差大 160~4200× |
| *Calibrating the Checksum*（bfloat16 GEMM 的经验误报界） | arXiv 2610.02240, 2026 | 经典界要求 `Ku ≤ 0.01`，bf16 下 `Ku ≈ 32` —— 阈值不是保守，是**无定义** |

**读数**：

- 我们的 κ=32 对实测残差保守 **四个数量级**，这件事本身**不是新发现**
  （V-ABFT 一族 2026 年已经在做更紧的阈值），所以 κ 标定只能当"消除常数"写，
  不能当贡献写。
- 还没被占的位置是**阈值的双边界**：下界由舍入误差给（Higham-Mary 的 `λ√n u`），
  **上界由应用可接受误差 Q 给**。τ < Q 的那一段是在为"本来无害的错误"付修复代价。
  现有工作（含 V-ABFT）只优化了下界那一侧。
- 收益随规模增长：`κn / (λ√n) = (κ/λ)√n`，N=256 时约 100×，即 6.7 个尾数位。
