#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* The Linux kernel's trick: embed the link in your struct; recover the struct from the link. */
struct list_head { struct list_head *next, *prev; };

#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define LIST_HEAD_INIT(name) { &(name), &(name) }

static void list_add_tail(struct list_head *item, struct list_head *head) {
    item->prev = head->prev; item->next = head;
    head->prev->next = item; head->prev = item;
}
static void list_del(struct list_head *item) {
    item->prev->next = item->next; item->next->prev = item->prev;
    item->next = item->prev = item;
}
#define list_for_each(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)

struct task {
    int id;
    const char *name;
    struct list_head run_list;      /* the same task can also sit on other lists via other members */
    struct list_head wait_list;
};

int main(void) {
    struct list_head runnable = LIST_HEAD_INIT(runnable), waiting = LIST_HEAD_INIT(waiting);
    struct task t[4] = { {.id = 1, .name = "init"}, {.id = 2, .name = "sshd"}, {.id = 3, .name = "cron"}, {.id = 4, .name = "nginx"} };
    for (int i = 0; i < 4; i++) list_add_tail(&t[i].run_list, &runnable);
    list_add_tail(&t[2].wait_list, &waiting);          /* cron is on two lists at once, with no extra allocation */

    struct list_head *p;
    printf("runnable:"); list_for_each(p, &runnable) printf(" %s", container_of(p, struct task, run_list)->name);
    printf("\nwaiting: "); list_for_each(p, &waiting) printf(" %s", container_of(p, struct task, wait_list)->name);
    list_del(&t[1].run_list);                           /* O(1) removal given only the node */
    printf("\nafter list_del(sshd):"); list_for_each(p, &runnable) printf(" %s", container_of(p, struct task, run_list)->name);
    puts("");
    printf("offsetof(run_list)=%zu offsetof(wait_list)=%zu\n", offsetof(struct task, run_list), offsetof(struct task, wait_list));
    return 0;
}
