# GPU集群调度算法

算法课设项目，在线GPU集群任务调度问题。

## 问题描述

服务器有GPU/CPU/内存资源，任务有到达时间/GPU需求/权重等属性。每个时间点决策：在能放下的服务器中选择，并给任务GPU切片分配。最小化加权等待时间、GPU闲置内存和整体完工时间（E_wait / E_memory / E_finish）。

## 构建与运行

```bash
mkdir build && cd build
cmake .. -G "Unix Makefiles"
make -j$(nproc)

# 单case运行
./gpu_scheduler ../data/case001.txt

# 批量评测
cd ..
python batch_evaluate.py
```

需要 C++17，推荐 MinGW-w64 或 MSVC。

## 分支策略

| 分支 | 用途 |
|------|------|
| `main` | 老师原始代码，不动 |
| `feature/pengxinyu` | 日常开发，所有版本push到这里 |
| `develop` | 只放各代最终选定版 |

## 算法迭代

| 版本 | 排序 | 选服务器 | E_wait | 状态 |
|------|------|----------|--------|------|
| v1.0 | release_time→duration | First-Fit | 335M | 基线 |
| v2.0 | WSPT | First-Fit | 178M | 采用 |
| v3.1 | WSPT | Best-Fit | 172M | 当前最优 |
| v4.2 | GPU-WSPT | Best-Fit | 169M | 最新候选 |

详细记录见 [changelog.md](changelog.md)。

## 分工

| 角色 | 成员 | 职责 |
|------|------|------|
| A 算法 | 彭欣雨 | 改scheduler.cpp，push + 更新changelog |
| B 测试 | 王湛溶 | 跑批量评测，出对比报告 |
| C 报告 | 曹庆庆 | 写英文报告 + 答辩PPT |

详见 [分工接口.txt](分工接口.txt)。

## 关键节点

| 日期 | 事项 |
|------|------|
| 6/24 | 测试赛1 |
| 6/28 | 测试赛2 |
| 7/8 | 正式赛提交 |
| 7/9 | 答辩 |
