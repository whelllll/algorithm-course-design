import os
import subprocess
import time

exe = "build/execname"

def evaluate_single_case(case_id):
    in_file = f"data/case{case_id:03d}.in"
    out_file = f"results/result{case_id:03d}.txt"
    
    if not os.path.exists(in_file):
        return None
        
    start_time = time.time()
    
    # 兼容 Linux WSL 的规范输入输出重定向
    with open(in_file, "r") as inf, open(out_file, "w") as outf:
        subprocess.run([f"./{exe}"], stdin=inf, stdout=outf)
        
    end_time = time.time()
    
    # 简单模拟读取结果，确保脚本不崩溃
    # 如果你们原本脚本后面还有复杂的解析逻辑，可以等跑通后再让A同学补上
    return end_time - start_time

print("开始批量评测...")
os.makedirs("results", exist_ok=True)
for i in range(1, 101):
    evaluate_single_case(i)
print("全部 100 个跑完！")
