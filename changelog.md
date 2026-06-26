# 算法迭代记录

## 版本总览

| 版本 | 日期 | 排序 | 选服务器 | E_wait | E_memory | E_finish | 状态 |
|------|------|------|----------|--------|----------|----------|------|
| v1.0 | 06-18 | release_time→duration | First-Fit | 335,177,183 | 162.74 | 29,947 | 起点 |
| v2.0 | 06-20 | **WSPT** | First-Fit | 177,961,377 | 161.21 | 29,483 | |
| v3.0 | 06-20 | 纯权重优先 | First-Fit | 207,599,369 | 161.77 | 30,065 | |
| **v3.1** | **06-20** | **WSPT** | **Best-Fit** | **171,930,372** | **160.21** | **28,873** | |
| v4.0 | 06-21 | WSPT | Best-Fit + hold | — | — | — | 死锁 |
| v4.1 | 06-21 | WSPT | Best-Fit + 避让 | 171,930,371 | 160.21 | 28,872 | 无效 |
| v4.2 | 06-21 | GPU-WSPT (min_gpu) | Best-Fit | 168,518,611 | 161.42 | 29,022 | |
| v4.3 | 06-21 | GPU-WSPT (gpu_memory) | Best-Fit | 169,631,683 | 161.90 | 29,427 | 退步 |
| v4.4 | 06-21 | GPU-WSPT (min_gpu) | Worst-Fit | 186,806,096 | 166.50 | 32,171 | 退步 |
| **v5.0** | **06-26** | **WSPT+时间感知** | **Best-Fit** | **112,236,844** | **40.15** | **22,498** | **← 当前** |

> **注意**：v5.0起E_memory改用新版按任务公式（对齐比赛修正标准），与v1.0-v4.4的旧口径不可直接对比。

---

## v5.0 — WSPT+时间感知优先级（2026-06-26）← 当前

### 改了什么

三个改动，核心是**时间感知优先级**：

1. 排序公式从 `weight/(duration×min_gpu)` 改回纯WSPT `weight/duration`
2. pending队列从静态优先队列改为**每轮按已等待时间重新排序**
3. 去掉几乎不触发的避让逻辑（v4.1）

### 原理

v4.2用GPU惩罚因子（min_gpu）压低多GPU任务优先级，但Best-Fit已经通过紧凑分配在控GPU。两者叠加是双重惩罚，多GPU任务被过度推迟。

更关键的是，静态优先队列里一个任务等再久优先级也不变。新方案每轮重算：

```
动态优先级 = weight × (已等待时间 + 1) / duration
```

已等待时间从`current_time - release_time`得来。等久了的任务自动提权，不会再被永久饿死。

### 结果（100个case平均，新版E_memory公式）

| 指标 | v4.2 | v5.0 | 变化 |
|------|------|------|------|
| E_wait | 168,518,611 | 112,236,844 | **-33.4%** |
| E_memory | 41.59 | 40.15 | **-3.5%** |
| E_finish | 29,022 | 22,498 | **-22.5%** |

**三项指标全部改善。** 没有之前v4.2那种trade-off——去掉GPU惩罚+时间感知让短任务快速出清，长任务也不被饿死。

### 修改文件

- `src/scheduler.h`：tryStartPendingJobs签名改为`vector<Job>&`
- `src/scheduler.cpp`：CompareJobPriority改回纯WSPT，tryStartPendingJobs重写为时间感知排序+逐任务尝试

---

## v4.4 — Worst-Fit选服务器（2026-06-21）

### 改了什么

在v4.2基础上，选服务器从Best-Fit改为Worst-Fit。

```
Best-Fit：  选放下后GPU剩余最少的（塞紧）
Worst-Fit： 选放下后GPU剩余最多的（留余地）
```

### 结果

| 指标 | v4.2 Best-Fit | v4.4 Worst-Fit | 变化 |
|------|-------------|---------------|------|
| E_wait | 168,518,611 | 186,806,096 | **+10.9%** |
| E_memory | 161.42 | 166.50 | +3.1% |
| E_finish | 29,022 | 32,171 | +10.9% |

三项全面退步。Worst-Fit留余地的策略在此场景无效——服务器GPU资源多了就浪费，不如Best-Fit塞紧减少碎片。

---

## v4.3 — GPU加权WSPT用gpu_memory（2026-06-21）

### 改了什么

在v4.2基础上，把 min_gpu 换成 gpu_memory。

```
v4.2  GPU-WSPT(min_gpu)：  优先级 = weight / (duration × min_gpu)
v4.3  GPU-WSPT(memory)：   优先级 = weight / (duration × gpu_memory)
```

### 结果

| 指标 | v4.2 | v4.3 | 变化 |
|------|------|------|------|
| E_wait | 168,518,611 | 169,631,683 | +0.7% |
| E_memory | 161.42 | 161.90 | +0.3% |
| E_finish | 29,022 | 29,427 | +1.4% |

三项全不如v4.2。min_gpu比gpu_memory更适合做GPU资源因子。

---

## v4.2 — GPU加权WSPT（2026-06-21）

### 改了什么

排序公式从 `weight/duration` 改为 `weight/(duration × min_gpu)`。

```
v3.1  WSPT：       优先级 = weight / duration
v4.2  GPU-WSPT：   优先级 = weight / (duration × min_gpu)
```

直觉：两个任务weight和duration一样，一个吃1张GPU、一个吃8张GPU。吃1张的先跑——占资源少，跑完释放快。

### 结果（100个case平均）

| 指标 | v3.1 | v4.2 | 变化 |
|------|------|------|------|
| E_wait | 171,930,372 | 168,518,611 | **-2.0%** |
| E_memory | 160.21 | 161.42 | +0.8% |
| E_finish | 28,873 | 29,022 | +0.5% |

E_wait继续降2%，E_memory和E_finish小幅退步。三个指标出现trade-off——GPU轻量任务优先减少等待但轻微牺牲利用率和总完成时间。

### 修改文件

- `src/scheduler.cpp`：CompareJobPriority 公式，分母加入 min_gpu

---

## v4.1 — 避让服务器（2026-06-21）

### 思路

不改变"塞不塞"，改"塞哪台"——如果下一个到达的高优先级任务只能跑在某台服务器上，当前任务选其他服务器，把那台留给他。

### 触发条件

下一个任务15秒内到 + WSPT高2倍 + 唯一可行服务器 → 避开那台

### 结果

100/100全过，但三个指标和v3.1几乎一模一样。触发条件在实际数据中极少满足。

### 修改文件

- `src/scheduler.h`：新增 shouldAvoidServer / tryStartOneJobAvoid
- `src/scheduler.cpp`：实现避让服务器逻辑
- `src/machine_state.h`：加了 remainingCpu() / remainingMemory()

---

## v4.0 — Hold暂停放行（2026-06-21）

### 思路

塞之前看一眼下一个到达的任务——如果它WSPT高很多且马上就到，故意不塞当前任务，空着服务器等。

### 结果

大面积死锁（72/100 case崩溃）。hold造成特殊执行顺序，running_heap空但服务器资源未回收。

---

## v3.1 — WSPT + Best-Fit选服务器（2026-06-20）← 当前最优

### 改了什么

排序保持WSPT不变，选服务器从 **First-Fit** 改为 **Best-Fit**。

```
First-Fit（旧）：遍历服务器，第一个能跑的就放
Best-Fit（新） ：遍历所有服务器，选"放下后GPU剩余最少"的那台
```

### 原理

First-Fit对第一台服务器有偏好，导致第一台塞满、后面的服务器闲着（E_finish差）。Best-Fit通过"选最紧的能放下的地方"，减少资源碎片，让GPU分配更均匀。

### 结果（100个case平均）

| 指标 | v2.0 | v3.1 | 变化 |
|------|------|------|------|
| E_wait | 177,961,377 | 171,930,372 | **-3.4%** |
| E_memory | 161.21 | 160.21 | -0.6% |
| E_finish | 29,483 | 28,873 | **-2.1%** |

**三个指标全面提升。**

### 修改文件

- `src/scheduler.cpp`：tryStartOneJob 从 first-fit 改为 best-fit
- `src/machine_state.h/cpp`：新增 remainingGpu() 方法

---

## v3.0 — 纯权重优先排序 · 未采用（2026-06-20）

### 尝试内容

受Tiresias启发，测试纯权重优先排序：先按 weight 降序，weight相同时再按 duration 升序。

### 与WSPT的区别

```
v2.0 WSPT：    优先级 = weight / duration（综合权衡）
v3.0 纯权重：  优先级 = weight（VIP绝对优先）
```

### 结果（100个case平均）

| 指标 | v2.0 | v3.0 | 变化 |
|------|------|------|------|
| E_wait | 177,961,377 | 207,599,369 | **+16.6%** |
| E_memory | 161.21 | 161.77 | +0.3% |
| E_finish | 29,483 | 30,065 | +2.0% |

### 分析

纯权重优先导致低权重短任务被长时间搁置，短任务本可以快速完成释放资源，反而拖累整体。WSPT通过"权重/时长"在重要性和快速完成之间取得了更好的平衡。

---

## v2.0 — WSPT排序（2026-06-20）

### 为什么从v1.0升级

v1.0先到先服务完全不考虑任务的重要性和紧急程度。高权重任务可能等很久。核心思路：**权重高、耗时短的任务优先执行。**

### 公式

优先级 = weight / duration

直觉：VIP且只点一碗面 → 先上。

### 效果（100个case平均）

| 指标 | v1.0 | v2.0 | 变化 |
|------|------|------|------|
| E_wait | 335,177,183 | 177,961,377 | **-47.0%** |
| E_memory | 162.74 | 161.21 | -0.9% |
| E_finish | 29,947 | 29,483 | -1.5% |

### 修改文件

- `src/scheduler.h`：新增 CompareJobPriority 比较器，等待队列改为优先队列
- `src/scheduler.cpp`：实现WSPT比较逻辑
- `evaluate.py` / `batch_evaluate.py`：新增评测脚本

---

## v1.0 — 基线（2026-06-18）

老师提供的 C++ CMake 示例代码。

- **排序**：release_time → duration → job_id（先到先服务）
- **选服务器**：First-Fit
- **算法**：纯贪心，无回溯

---

## 文献参考

| 文献 | 说明 |
|------|------|
| **Pinedo, M.L.** (2016). *Scheduling: Theory, Algorithms, and Systems* (5th ed.). Springer. | 调度理论教材，WSPT规则 |
| **Smith, W.E.** (1956). *Naval Research Logistics Quarterly*, 3(1-2), 59-66. | WSPT原始论文 |
| **Gu, J. et al.** (2019). Tiresias: A GPU cluster manager for distributed deep learning. *NSDI'19*. | GPU集群调度，WSPT变种 |
| **Xiao, W. et al.** (2018). Gandiva: Introspective cluster scheduling for deep learning. *OSDI'18*. | 异构GPU调度 |
| **Graham, R.L.** (1966). *Bell System Technical Journal*. | List Scheduling奠基 |

## 后续方向

| 类别 | 方向 | 说明 |
|------|------|------|
| 排序 | GPU加权WSPT | weight / (duration × GPU因素) |
| 排序 | 多因素加权 | α×WSPT + β×资源紧张度 |
| 选服务器 | Worst-Fit | 选剩余最多的，给大任务留余地 |
| 选服务器 | 负载均衡 | 优先放任务最少的服务器 |
| 元启发 | 模拟退火 | 贪心初始解 → 扰动 → 偶尔接受差解 |
| 元启发 | 迭代局部搜索 | 贪心 → 扰动 → 局部优化 → 重复 |
