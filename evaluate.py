import sys

def read_input(in_file):
    """读取输入文件"""
    with open(in_file) as f:
        lines = f.read().strip().split('\n')

    server_count, job_count = map(int, lines[0].split())
    servers = []
    for i in range(1, server_count + 1):
        gpu, mem, cpu, ram = map(int, lines[i].split())
        servers.append({'id': i, 'gpu_count': gpu, 'gpu_memory': mem, 'cpu': cpu, 'memory': ram})

    jobs = []
    for i in range(server_count + 1, server_count + 1 + job_count):
        rt, dur, mg, gm, cpu, ram, w = map(int, lines[i].split())
        jobs.append({'id': i - server_count, 'release': rt, 'duration': dur,
                     'min_gpu': mg, 'gpu_memory': gm, 'cpu': cpu, 'memory': ram, 'weight': w})
    return servers, jobs

def read_output(out_file, jobs):
    """读取输出文件，返回每个任务的调度记录"""
    with open(out_file) as f:
        lines = f.read().strip().split('\n')

    records = {}
    for line in lines:
        parts = list(map(int, line.split()))
        job_id = parts[0]
        server_id = parts[1]
        start_time = parts[2]
        gpu_used = parts[3]
        finish_time = parts[4]
        records[job_id] = {'server': server_id, 'start': start_time,
                           'gpu_used': gpu_used, 'finish': finish_time}
    return records

def evaluate(in_file, out_file):
    servers, jobs = read_input(in_file)
    records = read_output(out_file, jobs)

    # E_wait: sum of (start_time - release_time) * weight
    e_wait = 0
    for job in jobs:
        rec = records[job['id']]
        wait = rec['start'] - job['release']
        e_wait += wait * job['weight']

    # E_finish: makespan = max finish time
    e_finish = max(rec['finish'] for rec in records.values())

    # E_memory: average idle GPU memory
    # 收集所有时间点的事件
    events = []
    for job in jobs:
        rec = records[job['id']]
        events.append((rec['start'], 'start', job, rec))
        events.append((rec['finish'], 'finish', job, rec))
    events.sort(key=lambda x: (x[0], 0 if x[1] == 'finish' else 1))

    # 初始化每个服务器的总GPU显存和当前使用量
    server_total_mem = {s['id']: s['gpu_count'] * s['gpu_memory'] for s in servers}
    server_gpu_mem = {s['id']: s['gpu_memory'] for s in servers}
    server_used_mem = {s['id']: 0 for s in servers}

    total_idle_mem_time = 0
    prev_time = events[0][0]

    for time, etype, job, rec in events:
        dt = time - prev_time
        if dt > 0:
            # 计算这段时间所有服务器的空闲GPU显存
            idle = sum(server_total_mem[sid] - server_used_mem[sid] for sid in server_total_mem)
            total_idle_mem_time += idle * dt
        prev_time = time

        sid = rec['server']
        gpu_mem_used = rec['gpu_used'] * server_gpu_mem[sid]
        if etype == 'start':
            server_used_mem[sid] += gpu_mem_used
        else:
            server_used_mem[sid] -= gpu_mem_used

    total_time = events[-1][0] - events[0][0]
    if total_time > 0:
        e_memory = total_idle_mem_time / total_time
    else:
        e_memory = 0

    server_count = len(servers)
    e_memory_avg = e_memory / server_count if server_count > 0 else 0

    return e_wait, e_memory_avg, e_finish

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("用法: python evaluate.py data/case001.in output.txt")
        sys.exit(1)

    e_wait, e_memory, e_finish = evaluate(sys.argv[1], sys.argv[2])
    print(f"E_wait   (加权等待时间): {e_wait}")
    print(f"E_memory (平均GPU显存空闲): {e_memory:.2f}")
    print(f"E_finish (总完成时间): {e_finish}")
