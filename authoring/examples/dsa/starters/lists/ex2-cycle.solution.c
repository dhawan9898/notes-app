int has_cycle(const Node *head) {
    const Node *slow = head, *fast = head;           /* Floyd: the hare gains one node per step and must meet the tortoise */
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) return 1;
    }
    return 0;
}
