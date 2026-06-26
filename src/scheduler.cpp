#include "scheduler.h"

#include <algorithm>
#include <stdexcept>

using namespace std;

bool compareServerById(const ServerSpec &a, const ServerSpec &b) {
    return a.server_id < b.server_id;
}

bool compareJobByRelease(const Job &a, const Job &b) {
    if (a.release_time != b.release_time) return a.release_time < b.release_time;
    if (a.duration != b.duration) return a.duration < b.duration;
    return a.job_id < b.job_id;
}

bool CompareJobPriority::operator()(const Job &a, const Job &b) const {
    double ratio_a = static_cast<double>(a.weight) / a.duration;
    double ratio_b = static_cast<double>(b.weight) / b.duration;
    if (ratio_a != ratio_b) return ratio_a < ratio_b;
    return a.job_id > b.job_id;
}

bool FinishEvent::operator>(const FinishEvent &other) const {
    if (finish_time != other.finish_time) return finish_time > other.finish_time;
    if (server_id != other.server_id) return server_id > other.server_id;
    return job_id > other.job_id;
}

GreedyScheduler::GreedyScheduler(vector<ServerSpec> input_servers, vector<Job> input_jobs)
    : servers(move(input_servers)), jobs(move(input_jobs)) {
    sort(servers.begin(), servers.end(), compareServerById);
    sort(jobs.begin(), jobs.end(), compareJobByRelease);

    for (const auto &server : servers) {
        machines.emplace_back(server);
    }
    for (int index = 0; index < static_cast<int>(machines.size()); ++index) {
        machine_index_by_id[machines[index].spec.server_id] = index;
    }

    buildFeasibleMachines();
}

vector<ScheduleRecord> GreedyScheduler::schedule() {
    if (jobs.empty()) {
        return {};
    }

    long long current_time = jobs.front().release_time;
    int next_job_index = 0;
    vector<Job> pending_jobs;
    unordered_map<int, ScheduleRecord> records;
    priority_queue<FinishEvent, vector<FinishEvent>, greater<FinishEvent>> running_heap;

    while (static_cast<int>(records.size()) < static_cast<int>(jobs.size())) {
        releaseFinishedJobs(current_time, running_heap);

        while (next_job_index < static_cast<int>(jobs.size()) &&
               jobs[next_job_index].release_time <= current_time) {
            pending_jobs.push_back(jobs[next_job_index]);
            ++next_job_index;
        }

        tryStartPendingJobs(pending_jobs, current_time, next_job_index, records, running_heap);

        if (static_cast<int>(records.size()) == static_cast<int>(jobs.size())) {
            break;
        }

        if (running_heap.empty() && next_job_index >= static_cast<int>(jobs.size())) {
            throw runtime_error("Deadlock: no future event exists");
        }

        current_time = nextEventTime(current_time, next_job_index, running_heap);
    }

    vector<ScheduleRecord> ordered_records;
    ordered_records.reserve(records.size());
    for (int job_id = 1; job_id <= static_cast<int>(jobs.size()); ++job_id) {
        ordered_records.push_back(records.at(job_id));
    }
    return ordered_records;
}

void GreedyScheduler::buildFeasibleMachines() {
    for (const auto &job : jobs) {
        vector<pair<int, int>> entries;

        for (int index = 0; index < static_cast<int>(machines.size()); ++index) {
            int gpu_used = machines[index].requiredGpuCount(job);
            if (machines[index].canEverRun(job, gpu_used)) {
                entries.push_back({index, gpu_used});
            }
        }

        if (entries.empty()) {
            throw runtime_error("A job cannot run on any server.");
        }

        feasible_machines[job.job_id] = entries;
    }
}

void GreedyScheduler::releaseFinishedJobs(
    long long current_time,
    priority_queue<FinishEvent, vector<FinishEvent>, greater<FinishEvent>> &running_heap
) {
    while (!running_heap.empty() && running_heap.top().finish_time <= current_time) {
        FinishEvent event = running_heap.top();
        running_heap.pop();

        int machine_index = machine_index_by_id.at(event.server_id);
        machines[machine_index].releaseJob(event.running_job);
    }
}

void GreedyScheduler::tryStartPendingJobs(
    vector<Job> &pending_jobs,
    long long current_time,
    int next_job_index,
    unordered_map<int, ScheduleRecord> &records,
    priority_queue<FinishEvent, vector<FinishEvent>, greater<FinishEvent>> &running_heap
) {
    if (pending_jobs.empty()) return;

    sort(pending_jobs.begin(), pending_jobs.end(),
        [current_time](const Job &a, const Job &b) {
            long long wait_a = current_time - a.release_time;
            long long wait_b = current_time - b.release_time;
            if (wait_a < 0) wait_a = 0;
            if (wait_b < 0) wait_b = 0;
            double priority_a = static_cast<double>(a.weight) * (wait_a + 1) / a.duration;
            double priority_b = static_cast<double>(b.weight) * (wait_b + 1) / b.duration;
            if (priority_a != priority_b) return priority_a > priority_b;
            return a.job_id < b.job_id;
        });

    vector<Job> still_pending;
    for (auto &job : pending_jobs) {
        auto started = tryStartOneJob(job, current_time);
        if (started.has_value) {
            records[job.job_id] = started.record;
            running_heap.push(
                FinishEvent{
                    started.running_job.finish_time,
                    started.running_job.server_id,
                    started.running_job.job_id,
                    started.running_job,
                }
            );
        } else {
            still_pending.push_back(job);
        }
    }
    pending_jobs = move(still_pending);
}

int GreedyScheduler::shouldAvoidServer(const Job &current_job, long long current_time,
                                       int next_job_index) const {
    if (next_job_index >= static_cast<int>(jobs.size())) {
        return -1;
    }

    const Job &next_job = jobs[next_job_index];
    long long wait_time = next_job.release_time - current_time;
    if (wait_time <= 0 || wait_time > 15) {
        return -1;
    }

    double current_wsp = static_cast<double>(current_job.weight) / current_job.duration;
    double next_wsp = static_cast<double>(next_job.weight) / next_job.duration;
    if (next_wsp <= current_wsp * 2.0) {
        return -1;
    }

    const auto &next_feasible = feasible_machines.at(next_job.job_id);
    if (next_feasible.size() != 1) {
        return -1;
    }

    return machines[next_feasible[0].first].spec.server_id;
}

GreedyScheduler::StartResult GreedyScheduler::tryStartOneJobAvoid(
    const Job &job, long long current_time, int avoid_server) {
    const vector<pair<int, int>> &entries = feasible_machines.at(job.job_id);
    int best_index = -1;
    int best_gpu_used = 0;
    int best_remaining = -1;
    bool found_non_avoid = false;

    for (size_t idx = 0; idx < entries.size(); ++idx) {
        int machine_index = entries[idx].first;
        int gpu_used = entries[idx].second;
        if (!machines[machine_index].canStart(job, gpu_used)) {
            continue;
        }
        bool is_avoided = (machines[machine_index].spec.server_id == avoid_server);
        int remaining = machines[machine_index].remainingGpu() - gpu_used;

        if (!is_avoided && !found_non_avoid) {
            best_index = machine_index;
            best_gpu_used = gpu_used;
            best_remaining = remaining;
            found_non_avoid = true;
        } else if (is_avoided && found_non_avoid) {
            continue;
        } else if (best_index == -1 || remaining < best_remaining) {
            best_index = machine_index;
            best_gpu_used = gpu_used;
            best_remaining = remaining;
        }
    }

    if (best_index == -1) {
        return StartResult{};
    }
    pair<ScheduleRecord, RunningJob> result = machines[best_index].startJob(job, current_time, best_gpu_used);
    return StartResult{true, result.first, result.second};
}

GreedyScheduler::StartResult GreedyScheduler::tryStartOneJob(const Job &job, long long current_time) {
    const vector<pair<int, int>> &entries = feasible_machines.at(job.job_id);
    int best_index = -1;
    int best_gpu_used = 0;
    int best_waste = -1;
    int best_remaining = -1;
    for (size_t idx = 0; idx < entries.size(); ++idx) {
        int machine_index = entries[idx].first;
        int gpu_used = entries[idx].second;
        if (!machines[machine_index].canStart(job, gpu_used)) {
            continue;
        }
        int VG_si = machines[machine_index].spec.gpu_memory;
        int waste = gpu_used * VG_si - job.gpu_memory;
        if (waste < 0) waste = 0;
        int remaining = machines[machine_index].remainingGpu() - gpu_used;
        bool better = false;
        if (best_index == -1) {
            better = true;
        } else if (waste != best_waste) {
            better = waste < best_waste;
        } else {
            better = remaining < best_remaining;
        }
        if (better) {
            best_index = machine_index;
            best_gpu_used = gpu_used;
            best_waste = waste;
            best_remaining = remaining;
        }
    }
    if (best_index == -1) {
        return StartResult{};
    }
    pair<ScheduleRecord, RunningJob> result = machines[best_index].startJob(job, current_time, best_gpu_used);
    return StartResult{true, result.first, result.second};
}

long long GreedyScheduler::nextEventTime(
    long long current_time,
    int next_job_index,
    const priority_queue<FinishEvent, vector<FinishEvent>, greater<FinishEvent>> &running_heap
) const {
    vector<long long> candidates;

    if (next_job_index < static_cast<int>(jobs.size())) {
        candidates.push_back(jobs[next_job_index].release_time);
    }
    if (!running_heap.empty()) {
        candidates.push_back(running_heap.top().finish_time);
    }

    long long next_time = -1;
    for (long long candidate : candidates) {
        if (candidate <= current_time) {
            continue;
        }
        if (next_time == -1 || candidate < next_time) {
            next_time = candidate;
        }
    }

    if (next_time == -1) {
        throw runtime_error("No future event exists.");
    }

    return next_time;
}

