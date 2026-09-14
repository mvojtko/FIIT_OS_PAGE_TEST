#pragma once

#include "types.h"

typedef enum tSchedulingAlgorithm
{
    // Round robin scheduling algorithm (no priority).
    SCHEDULING_ALGORITHM_ROUND_ROBIN,

    // Round robin scheduling algorithm with priority (priority based on longer runtime of the task).
    SCHEDULING_ALGORITHM_ROUND_ROBIN_WITH_PRIORITY,

    // Random scheduling algorithm (priority based on multiple tickets assigned to the task).
    SCHEDULING_ALGORITHM_RANDOM_WITH_PRIORITY,

    // Fair share scheduling algorithm with priority Each user gets a fair share of the CPU.
    // (priority based on longer runtime of the task).
    SCHEDULING_ALGORITHM_FAIR_SHARE_WITH_PRIORITY,
} tSchedulingAlgorithm;

typedef struct tScheduler
{
    tPid current_task; // Current running task.
    int task_runtime; // Runtime of the task in current cycle.
    int total_runtime; // Total system runtime.
    const tSchedulingAlgorithm scheduling_algorithm; // Scheduling algorithm.
} tScheduler;

void init_scheduler(tScheduler *scheduler);

void destroy_scheduler();

// Selects the next task to run.
// This function:
//   - adds one to task_runtime.
//   - adds one to total_runtime.
//   - Selects the next task to run based on the scheduling algorithm.
//   - The result may be that the curent task is selected again or it is replaced by a new task.
//   - If the current task is not selected again its runtime is added to the tasks runtime.
// Returns:
//   - The PID of the next task to run.
//   - -1 if no task is available to run.
// Scheduling algorithms:
tPid schedule();
