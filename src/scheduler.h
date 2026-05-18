#pragma once

#include <stdint.h>

typedef struct task {
	uint64_t upid;
	uint64_t rsp;
	uint64_t cr3;
	struct task *next;
	struct task *prev;
	void* stack_base;
} task_t;

void scheduler_init(void);
void scheduler_start(void);
void scheduler_yield(void);
void scheduler_exit_task(void);

task_t* scheduler_create_task(void (*entry_point)(void));
void scheduler_enroll_task(task_t* new_task);

task_t* get_task_by_upid(uint64_t target_upid);
