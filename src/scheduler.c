#include "scheduler.h"

#include "cpu/IRQ.h"
#include <stddef.h>
#include "mem/pma.h"
#include "mem/heap.h"
#include "panic.h"
#include "mem/mem.h"
#include <string.h>
#include "mem/paging.h"
#include "cpu/coreinfo.h"

static uint64_t current_pid = 1;

static struct trap_frame* scheduler_timer_handler(struct trap_frame* tf) {
	task_t* current_task;
	GET_CURRENT_TASK(current_task);

	current_task->rsp = (uint64_t)tf;
	task_t* old_task = current_task;
	task_t* next_task = current_task->next;

	if (old_task->upid == 0 && old_task != next_task && old_task->next != NULL && old_task->prev != NULL) {
		old_task->prev->next = old_task->next;
		old_task->next->prev = old_task->prev;

		if (old_task->stack_base != NULL) {
			uintptr_t stack_phys = (uintptr_t)old_task->stack_base - hhdm_offset;
			pma_free_pages(stack_phys, 4);
		}

		free(old_task);
	}

	uint64_t current_pml4 = vmm_get_pml4();
	if (next_task->cr3 != current_pml4 && next_task->cr3 != 0) {
		vmm_set_pml4(next_task->cr3);
	}

	SET_CURRENT_TASK(next_task);
	return (struct trap_frame*)next_task->rsp;
}

task_t* get_task_by_upid(uint64_t target_upid) {
	task_t* current_task; GET_CURRENT_TASK(current_task);
	if (current_task == NULL) return NULL;
	task_t* start = current_task;
	task_t* curr = start;
	do {
		if (curr->upid == target_upid) return curr;
		curr = curr->next;
	} while (curr != start);
	return NULL;
}

task_t* scheduler_create_task(void (*entry_point)(void)) {
	task_t* new_task = (task_t*)zalloc(sizeof(task_t));
	if (!new_task) return NULL;

	new_task->upid = 0;
	new_task->cr3 = vmm_get_pml4(); // current page table as default

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

	new_task->next = NULL;
	new_task->prev = NULL;

	return new_task;
}

void scheduler_enroll_task(task_t* new_task) {
	__asm__ volatile ("cli");

	new_task->upid = current_pid++;

	task_t* current_task;
	GET_CURRENT_TASK(current_task);

	if (current_task == NULL) {
		SET_CURRENT_TASK(new_task);
		new_task->next = new_task;
		new_task->prev = new_task;
	} else {
		task_t* last = current_task->prev;
		last->next = new_task;
		new_task->prev = last;
		new_task->next = current_task;
		current_task->prev = new_task;
	}

	__asm__ volatile ("sti");
}

void scheduler_exit_task(void) {
	__asm__ volatile ("cli");

	task_t* current_task;
	GET_CURRENT_TASK(current_task);

	if (current_task->next == current_task) {
		panic("SCHEDULER", "Last task attempted to exit. Halting.");
	}

	current_task->upid = 0;

	__asm__ volatile ("int $32");
	panic("scheduler_exit_task", "execution should never get here");
}

void scheduler_start(void) {
	task_t* current_task; GET_CURRENT_TASK(current_task);
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

void scheduler_init(void) {
	task_t* bootstrap_task = (task_t*)zalloc(sizeof(task_t));
	bootstrap_task->upid = 0;
	bootstrap_task->cr3 = vmm_get_pml4();

	task_t* current_task; GET_CURRENT_TASK(current_task);

	SET_CURRENT_TASK(bootstrap_task);
	bootstrap_task->next = bootstrap_task;
	bootstrap_task->prev = bootstrap_task;

	coreinfo_t *my_cpu;
	GET_CURRENT_CPU(my_cpu);
	irq_install_handler(my_cpu->logical_id, 32, scheduler_timer_handler);
}
