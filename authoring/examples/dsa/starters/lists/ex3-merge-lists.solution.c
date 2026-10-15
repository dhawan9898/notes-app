Node *merge(Node *a, Node *b) {
    Node dummy = {0, 0, NULL}, *tail = &dummy;       /* a dummy head removes the "empty result" special case */
    while (a && b) {
        if (a->key <= b->key) { tail->next = a; a = a->next; }   /* <= keeps the merge stable */
        else { tail->next = b; b = b->next; }
        tail = tail->next;
    }
    tail->next = a ? a : b;
    return dummy.next;
}
