#include "scheduler.h"
#include "apic/lapic.h"
#include "cpu/IRQ.h"
#include <stddef.h>
#include "mem/pma.h"
#include "mem/heap.h" // IWYU pragma: keep
#include "kstate.h"
#include <string.h>

task_t *current_task = NULL;

struct trap_frame* scheduler_timer_handler(struct trap_frame* tf) {
	if (current_task != NULL) {
		current_task->rsp = (uint64_t)tf;
	}

	if (current_task != NULL && current_task->next != NULL) {
		current_task = current_task->next;
	}

	return (struct trap_frame*)current_task->rsp;
}

task_t* get_task_by_upid(uint64_t target_upid) {
	if (current_task == NULL) return NULL;
	task_t* start = current_task;
	task_t* curr = start;
	do {
		if (curr->upid == target_upid) return curr;
		curr = curr->next;
	} while (curr != start);
	return NULL;
}

task_t* scheduler_create_task(void (*entry_point)(void), uint64_t requested_upid) {
	if (get_task_by_upid(requested_upid) != NULL) {
		panic("SCHEDULER", "UPID COLLISION");
	}

	task_t* new_task = (task_t*)zalloc(sizeof(task_t));
	new_task->upid = requested_upid;

	uint64_t stack_phys = pma_alloc_pages(4);
	uint64_t stack_top = stack_phys + (4 * 4096) + hhdm_offset;
	new_task->stack_base = (void*)(stack_phys + hhdm_offset);

	struct trap_frame* frame = (struct trap_frame*)(stack_top - sizeof(struct trap_frame));
	memset(frame, 0, sizeof(struct trap_frame));

	frame->rip = (uint64_t)entry_point;
	frame->cs  = 0x08;
	frame->ss  = 0x10;
	frame->rsp = stack_top;
	frame->rflags = 0x202;

	new_task->rsp = (uint64_t)frame;

	// enroll task
	if (current_task == NULL) {
		current_task = new_task;
		new_task->next = new_task;
		new_task->prev = new_task;
	} else {
		task_t* last = current_task->prev;
		last->next = new_task;
		new_task->prev = last;
		new_task->next = current_task;
		current_task->prev = new_task;
	}

	return new_task;
}

void scheduler_start(void) {
	if (current_task == NULL) {
		panic("SCHEDULER", "INITALIZED WITH NO TASKS");
	}

	__asm__ volatile ("sti"); // ensure interrupts
	while(1) {
		__asm__ volatile ("hlt");
	}
}

void scheduler_yield(void) {
	__asm__ volatile ("int $32");
}

void scheduler_init() {
	task_t* ktask = (task_t*)zalloc(sizeof(task_t));

	current_task = ktask;
	ktask->next = ktask;
	ktask->prev = ktask;

	irq_install_handler(lapic_get_id(), 32, scheduler_timer_handler);
}
