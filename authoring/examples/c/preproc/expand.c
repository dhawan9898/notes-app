// Expansion order, step by step. Run through `cc -E -P` to see what the compiler receives.
#define VERSION 3
#define XSTR(x) #x              // stringifies its argument exactly as written
#define STR(x) XSTR(x)          // the argument is macro-expanded first, then passed on
const char *a = XSTR(VERSION);  // "VERSION"
const char *b = STR(VERSION);   // "3"

#define CAT(a, b) a ## b
#define XCAT(a, b) CAT(a, b)
int CAT(var_, VERSION);         // var_VERSION: ## pastes before expansion
int XCAT(var_, VERSION);        // var_3

#define foo foo + 1             // a macro is not re-expanded inside its own expansion
int foo_result = foo;           // foo + 1, not infinite recursion

#define TWICE(x) ((x) + (x))
#define ONE 1
int r = TWICE(TWICE(ONE));      // arguments are fully expanded first, then rescanned

#define F(x) G(x)
#define G(x) [x]
int arr[] = F(F(2));            // each level of the chain is rescanned
