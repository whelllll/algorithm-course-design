import subprocess, sys, os

exe = "build\\execname.exe"
data_dir = "data"
result_dir = sys.argv[1] if len(sys.argv) > 1 else "results"

os.makedirs(result_dir, exist_ok=True)

# 跑所有100个case
for i in range(1, 101):
    in_file = f"{data_dir}\\case{i:03d}.in"
    out_file = f"{result_dir}\\result_{i:03d}.txt"
    subprocess.run(f'cmd /c "{exe} < {in_file} > {out_file}"', shell=True, capture_output=True)
    if i % 20 == 0:
        print(f"已跑完 {i}/100")

print("全部100个跑完")

# 评分
import evaluate
total_wait = 0
total_memory = 0
total_finish = 0
success = 0

for i in range(1, 101):
    in_file = f"{data_dir}\\case{i:03d}.in"
    out_file = f"{result_dir}\\result_{i:03d}.txt"
    try:
        w, m, f = evaluate.evaluate(in_file, out_file)
        total_wait += w
        total_memory += m
        total_finish += f
        success += 1
    except Exception as e:
        print(f"case{i:03d} 出错: {e}")

if success > 0:
    print(f"\n{'='*50}")
    print(f"成功评测: {success}/100 个case")
    print(f"平均 E_wait   : {total_wait / success:.2f}")
    print(f"平均 E_memory : {total_memory / success:.2f}")
    print(f"平均 E_finish : {total_finish / success:.2f}")
