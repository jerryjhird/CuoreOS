#pragma once

#include <stdint.h>

typedef struct task {
	uint64_t upid;
	uint64_t rsp;
	struct task *next;
	struct task *prev;
	void* stack_base;
} task_t;

extern task_t *current_task;

void scheduler_init(void);
void scheduler_start(void);
void scheduler_yield(void);

task_t* scheduler_create_task(void (*entry_point)(void), uint64_t requested_upid);
