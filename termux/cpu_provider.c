#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define MAX_CORES 64
#define USER_HZ 100
#define HASH_SIZE 8192

typedef struct TaskNode {
    int pid;
    int tid;
    long long last_ticks;
    struct TaskNode* next;
} TaskNode;

TaskNode* hash_table[HASH_SIZE];

unsigned int hash(int pid, int tid) {
    return (unsigned int)(pid ^ (tid << 5)) % HASH_SIZE;
}

long long update_task(int pid, int tid, long long ticks, int core, long long* core_diffs) {
    unsigned int h = hash(pid, tid);
    TaskNode* node = hash_table[h];
    while (node) {
        if (node->pid == pid && node->tid == tid) {
            long long diff = ticks - node->last_ticks;
            node->last_ticks = ticks;
            if (diff > 0 && core >= 0 && core < MAX_CORES) {
                core_diffs[core] += diff;
            }
            return diff;
        }
        node = node->next;
    }
    node = malloc(sizeof(TaskNode));
    node->pid = pid;
    node->tid = tid;
    node->last_ticks = ticks;
    node->next = hash_table[h];
    hash_table[h] = node;
    return 0;
}

int main() {
    long long core_diffs[MAX_CORES];
    struct timeval last_time, curr_time;
    gettimeofday(&last_time, NULL);

    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores > MAX_CORES) num_cores = MAX_CORES;

    while (1) {
        memset(core_diffs, 0, sizeof(core_diffs));
        
        DIR* proc_dir = opendir("/proc");
        if (!proc_dir) return 1;

        struct dirent* proc_entry;
        while ((proc_entry = readdir(proc_dir))) {
            if (proc_entry->d_name[0] < '0' || proc_entry->d_name[0] > '9') continue;

            int pid = atoi(proc_entry->d_name);
            char task_path[256];
            snprintf(task_path, sizeof(task_path), "/proc/%s/task", proc_entry->d_name);
            DIR* task_dir = opendir(task_path);
            if (!task_dir) continue;

            struct dirent* task_entry;
            while ((task_entry = readdir(task_dir))) {
                if (task_entry->d_name[0] < '0' || task_entry->d_name[0] > '9') continue;

                int tid = atoi(task_entry->d_name);
                char stat_path[512];
                snprintf(stat_path, sizeof(stat_path), "%s/%s/stat", task_path, task_entry->d_name);
                
                int fd = open(stat_path, O_RDONLY);
                if (fd != -1) {
                    char buf[2048];
                    ssize_t n = read(fd, buf, sizeof(buf) - 1);
                    close(fd);
                    if (n > 0) {
                        buf[n] = '\0';
                        char* last_paren = strrchr(buf, ')');
                        if (last_paren) {
                            long long utime = 0, stime = 0;
                            int processor = -1;
                            int field = 1;
                            char* p = last_paren + 2;
                            char* saveptr;
                            char* token = strtok_r(p, " ", &saveptr);
                            while (token) {
                                if (field == 12) utime = atoll(token);
                                else if (field == 13) stime = atoll(token);
                                else if (field == 37) {
                                    processor = atoi(token);
                                    break;
                                }
                                token = strtok_r(NULL, " ", &saveptr);
                                field++;
                            }
                            update_task(pid, tid, utime + stime, processor, core_diffs);
                        }
                    }
                }
            }
            closedir(task_dir);
        }
        closedir(proc_dir);

        gettimeofday(&curr_time, NULL);
        double dt = (curr_time.tv_sec - last_time.tv_sec) + (curr_time.tv_usec - last_time.tv_usec) / 1000000.0;
        double hz_dt = dt * USER_HZ;

        printf("\033[H\033[J"); // Clear and home
        if (hz_dt > 0) {
            for (int i = 0; i < num_cores; i++) {
                double load = (core_diffs[i] / hz_dt) * 100.0;
                if (load > 100.0) load = 100.0;
                printf("%d: %.0f\n", i, load);
            }
        }
        fflush(stdout);

        last_time = curr_time;
        usleep(1000000);
    }

    return 0;
}