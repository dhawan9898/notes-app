int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

const Command table[] = {{"add", add}, {"sub", sub}, {"mul", mul}};
const size_t table_len = sizeof table / sizeof table[0];
