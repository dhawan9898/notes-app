Node *merge(Node *a, Node *b) {
    Node dummy = {0, 0, NULL}, *tail = &dummy;
    while (a && b) {
        if (a->key < b->key) { tail->next = a; a = a->next; }    /* < takes b first on ties */
        else { tail->next = b; b = b->next; }
        tail = tail->next;
    }
    tail->next = a ? a : b;
    return dummy.next;
}
