import os
import random

# 创建保存数据集的目录
DATA_DIR = "data"
os.makedirs(DATA_DIR, exist_ok=True)

print("开始生成 100 个分档数据集...")

for i in range(1, 101):
    lines = []

    # ================== 一、根据档次随机 M 和 N ==================
    if 1 <= i <= 20:  # 简单实例
        M = random.randint(2, 10)
        N = random.randint(20, 100)
    elif 21 <= i <= 60:  # 中等实例
        M = random.randint(11, 50)
        N = random.randint(101, 1000)
    elif 61 <= i <= 90:  # 困难实例
        M = random.randint(50, 100)
        N = random.randint(1001, 5000)
    else:  # 极端实例
        M = random.randint(50, 100)
        N = random.randint(1001, 5000)

    lines.append(f"{M} {N}")

    # ================== 二、服务器独立随机生成 ==================
    servers = []
    for _ in range(M):
        g_s = random.randint(1, 8)
        vg_s = random.randint(16, 80)
        c_s = random.randint(8, 128)
        r_s = random.randint(32, 1024)
        servers.append({"g": g_s, "vg": vg_s, "c": c_s, "r": r_s})
        lines.append(f"{g_s} {vg_s} {c_s} {r_s}")

    # ================== 三、任务生成与多卡有解校验 ==================
    for _ in range(N):
        retry = 0
        while True:
            # 全档统一参数范围，对齐规范
            r = random.randint(0, 20000)
            p = random.randint(1, 1000)
            w = random.randint(1, 20)
            g_i = random.randint(1, 8)
            v_i = random.randint(1, 640)
            c_i = random.randint(1, 128)
            m_i = random.randint(1, 1024)

            # 极端档 20% 概率生成瓶颈任务
            if i >= 91:
                prob = random.random()
                if prob < 0.05:
                    v_i = random.randint(320, 640)
                    w = 20
                elif prob < 0.10:
                    g_i = random.randint(4, 8)
                    w = 20
                elif prob < 0.15:
                    c_i = random.randint(64, 128)
                    w = 20
                elif prob < 0.20:
                    m_i = random.randint(512, 1024)
                    w = 20

            # 有解校验：是否存在某台服务器分配 u 张卡能满足
            is_valid = False
            for s in servers:
                if g_i <= s["g"] and c_i <= s["c"] and m_i <= s["r"]:
                    for u in range(g_i, s["g"] + 1):
                        if v_i <= u * s["vg"]:
                            is_valid = True
                            break
                if is_valid:
                    break

            if is_valid:
                break

            retry += 1
            if retry >= 10000:
                g_i = 1
                v_i = 1
                c_i = 1
                m_i = 1
                w = 1

        lines.append(f"{r} {p} {g_i} {v_i} {c_i} {m_i} {w}")

    # ================== 四、写入文件 ==================
    file_name = f"{DATA_DIR}/case{i:03d}.in"
    with open(file_name, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

print("完成！100 个测试集已生成。")
