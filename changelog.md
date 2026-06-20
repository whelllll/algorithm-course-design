# 算法迭代记录

---

## v3.0 — WSPT + Best-Fit选服务器（2026-06-20）

### 改了什么

排序保持WSPT不变，选服务器从 **First-Fit** 改为 **Best-Fit**。

```
First-Fit（旧）：遍历服务器，第一个能跑的就放
Best-Fit（新） ：遍历所有服务器，选"放下后GPU剩余最少"的那台
```

### 原理

First-Fit对第一台服务器有偏好，导致第一台塞满、后面的服务器闲着（E_finish差）。Best-Fit通过"选最紧的能放下的地方"，减少资源碎片，让GPU分配更均匀。


### 结果（100个case平均）

| 指标 | v2.0 WSPT | v3.0 Best-Fit | 变化 |
|------|-----------|-------------|------|
| E_wait | 177,961,377 | 171,930,372 | **-3.4%** |
| E_memory | 161.21 | 160.21 | -0.6% |
| E_finish | 29,483 | 28,873 | **-2.1%** |

**三个指标全面提升，Best-Fit有效。**

### 修改文件

- `src/scheduler.cpp`：tryStartOneJob 从 first-fit 改为 best-fit
- `src/machine_state.h/cpp`：新增 remainingGpu() 方法

---

## v3.0 — 权重优先排序（Tiresias风格）· 已废弃（2026-06-20）

### 尝试内容

受Tiresias(Gu et al., 2019)启发，测试纯权重优先排序：先按 weight 降序排列，weight相同时再按 duration 升序排列。

### 与WSPT的区别

```
WSPT：          优先级 = weight / duration（综合权衡）
Tiresias风格：   优先级 = weight（VIP绝对优先）
```

### 结果（100个case平均）

| 指标 | v2.0 WSPT | v3.0 权重优先 | 变化 |
|------|-----------|-------------|------|
| E_wait | 177,961,377 | 207,599,369 | **+16.6%（变差）** |
| E_memory | 161.21 | 161.77 | +0.3% |
| E_finish | 29,483 | 30,065 | +2.0% |

### 结论

**WSPT全面优于纯权重优先。** 纯权重优先会导致低权重短任务被长时间搁置，而短任务本可以快速完成释放资源，反而拖累整体。WSPT通过"权重/时长"的比值，在"重要性"和"快速完成"之间取得了更好的平衡。

**决策：放弃此方向，保留v2.0 WSPT。**

---

## v2.0 — WSPT优先队列调度（2026-06-20）

### 为什么从v1.0升级到v2.0？

v1.0的"先到先服务"完全不考虑任务的重要性和紧急程度。一个权重为1的短任务和一个权重为12的长任务同时到达，v1.0随意排序，导致高权重任务可能等很久，E_wait分数很差。

v2.0的核心思路：**让系统学会"插队"——权重高、耗时短的任务优先执行。**

### 和v1.0的区别

| | v1.0（老师基线） | v2.0（WSPT） |
|---|---|---|
| **排序规则** | 按到达时间排，先到先服务 | 按 weight/duration 优先，高权重短任务插队 |
| **等待队列** | 普通队列（FIFO，不能插队） | 优先队列（高优先级的自动排前面） |
| **选服务器** | 第一个能跑的就放（first-fit） | 同v1.0，未改 |
| **时间复杂度** | O(n*m)，n=任务数，m=服务器数 | 同v1.0，未增加 |

### 用到的算法：WSPT（Weighted Shortest Processing Time）

- **来源**：经典调度理论，Pinedo《Scheduling: Theory, Algorithms, and Systems》
- **公式**：任务优先级 = weight / duration
- **直觉**：餐厅VIP且只点一碗面的客人，先给他上菜
- **实现位置**：`src/scheduler.h` 第11-13行（CompareJobPriority），`src/scheduler.cpp` 第28-33行

### 效果（100个case平均）

| 指标 | v1.0基线 | v2.0 WSPT | 变化 |
|------|----------|-----------|------|
| E_wait | 335,177,183 | 177,961,377 | **-47.0%** |
| E_memory | 162.74 | 161.21 | -0.9% |
| E_finish | 29,947 | 29,483 | -1.5% |

**E_wait几乎砍半，另外两个指标也有小幅提升。三个指标全面优于基线。**

### 后续可以尝试的算法

#### 近期（继续改排序规则，不改架构）
| 排序策略 | 公式 | 适合场景 |
|----------|------|---------|
| 纯权重优先 | priority = weight | 极度重视VIP任务 |
| 短任务优先 | priority = -duration | 快速清空积压 |
| 资源紧张度优先 | weight / (gpu_memory * duration) | GPU显存是瓶颈时 |
| EDD规则 | priority = 1/(duration+release) | 考虑截止时间压力 |
| 多因素加权 | α×weight/duration + β×weight/gpu_memory | 综合权衡 |

#### 中期（改选服务器策略）
| 策略 | 说明 |
|------|------|
| Best-Fit | 不选第一个能跑的，选"剩余资源刚好够"的，减少碎片 |
| Worst-Fit | 选剩余资源最多的，留余地给大任务 |
| 负载均衡 | 优先把任务放到当前任务最少的服务器 |

#### 后期（元启发式算法，冲刺高分）
| 算法 | 说明 |
|------|------|
| 模拟退火 | 先贪心得到一个解，然后随机扰动，偶尔接受差解来跳出局部最优 |
| 迭代局部搜索 | 贪心解 → 扰动(swap/move) → 局部优化 → 重复，取最优 |
| 波束搜索 | 每次不只选一个任务，保留top-K个可能的分支继续搜 |

### 已知不足和改进方向

1. **选服务器还是first-fit**：目前只改了排序，选服务器仍然是"第一个能跑的"。改成best-fit可能再降E_memory。
2. **没有考虑任务之间的资源竞争**：当多个高权重任务都要大量GPU时，当前策略不会预留资源。
3. **极端case适配**：部分大case（如case090-100）GPU显存紧张，可能需要在排序中加入GPU资源因素。
4. **没有回溯**：当前是纯贪心，做了决定就不会改。元启发式可以通过"推翻重来"找到更好的解。

### 文献来源

**核心参考：WSPT规则**

| 文献 | 说明 |
|------|------|
| **Pinedo, M.L.** (2016). *Scheduling: Theory, Algorithms, and Systems* (5th ed.). Springer. | 调度理论经典教材，第3章系统介绍WSPT规则及其在多机调度中的启发式应用 |
| **Smith, W.E.** (1956). Various optimizers for single-stage production. *Naval Research Logistics Quarterly*, 3(1-2), 59-66. | WSPT的原始论文，首次证明按 weight/duration 排序可最小化加权完成时间之和 |

**延伸参考：GPU集群调度（场景最接近）**

| 文献 | 说明 |
|------|------|
| **Gu, J. et al.** (2019). Tiresias: A GPU cluster manager for distributed deep learning. *NSDI'19*. | 提出"预估时长 × 优先级"排序，本质为WSPT在GPU集群场景的变种 |
| **Xiao, W. et al.** (2018). Gandiva: Introspective cluster scheduling for deep learning. *OSDI'18*. | 异构GPU环境下的调度策略，处理GPU/显存异构性 |
| **Graham, R.L.** (1966). Bounds for certain multiprocessing anomalies. *Bell System Technical Journal*, 45(9), 1563-1581. | List Scheduling奠基之作，v1.0基线贪心调度的理论来源 |

### 修改文件

- `src/scheduler.h`：新增 CompareJobPriority 比较器，等待队列改为优先队列
- `src/scheduler.cpp`：实现WSPT比较逻辑
- `evaluate.py`：新增本地评分脚本
- `batch_evaluate.py`：新增批量跑分脚本

---

## v1.0 — 基线贪心调度器（老师示例代码）

### 算法描述

- **排序规则**：按 release_time → duration → job_id 排序（先到先服务）
- **选服务器策略**：First-Fit（遍历服务器列表，选第一个满足资源要求的）
- **算法类型**：纯贪心（Greedy），无回溯，无学习

### 来源

老师提供的 C++ CMake 示例代码

---
