#include "partition.h"

static partition_t* head = NULL;

void partition_register(partition_t* new_part) {
    if (!new_part) return;

    if (head == NULL) {
        head = new_part;
    } else {
        partition_t* current = head;
        while (current->next != NULL) {
            current = (partition_t*)current->next;
        }
        current->next = (struct partition_s*)new_part;
    }
}