# C Solver Data Structures

Complete reference for all data structures and types in the BSAT C solver.

## Core Types (include/types.h)

### Basic Types

```c
typedef uint32_t Var;      // Variable (1-indexed, 0 is invalid)
typedef int32_t  Lit;      // Literal (positive = var, negative = -var)
typedef uint32_t CRef;     // Clause reference (offset into arena)
typedef uint8_t  Level;    // Decision level (0-255)
typedef int8_t   lbool;    // 3-valued logic (-1=FALSE, 0=UNDEF, 1=TRUE)
```

**Constants:**
```c
#define INVALID_VAR  0
#define INVALID_LIT  0
#define INVALID_CREF UINT32_MAX

#define TRUE   1
#define FALSE  -1
#define UNDEF  0
```

### Literal Encoding

Literals use a compact integer encoding:
- Positive literal (variable x): `2*x`
- Negative literal (¬x): `2*x + 1`

**Helper macros:**
```c
var(lit)      // Extract variable from literal
sign(lit)     // Get sign (0=positive, 1=negative)
lit_make(v,s) // Create literal from variable and sign
lit_neg(lit)  // Negate a literal
```

**Example:**
```c
Var x = 5;
Lit pos = lit_make(x, 0);  // x5 → literal 10
Lit neg = lit_make(x, 1);  // ¬x5 → literal 11
Lit flipped = lit_neg(pos); // 10 → 11
```

---

## Variable Information (include/solver.h)

### VarInfo Structure

Per-variable state stored in solver→vars[v]:

```c
typedef struct VarInfo {
    // Current assignment
    lbool    value;          // TRUE, FALSE, or UNDEF
    Level    level;          // Decision level where assigned
    CRef     reason;         // Reason clause (INVALID_CREF for decisions)
    uint32_t trail_pos;      // Position in trail

    // Phase saving
    bool     polarity;       // Saved polarity for phase saving
    uint32_t last_polarity;  // Last conflict where polarity was saved

    // VSIDS activity
    double   activity;       // Variable activity score
    uint32_t heap_pos;       // Position in VSIDS heap
} VarInfo;
```

**Key fields:**
- **value**: Current assignment (TRUE/FALSE/UNDEF)
- **level**: Decision level (0 = level 0, >0 = search level)
- **reason**: CRef to implication clause (INVALID_CREF if decision)
- **polarity**: Saved phase for next decision
- **activity**: VSIDS score (higher = more active in conflicts)
- **heap_pos**: Position in binary max-heap for VSIDS

**Access pattern:**
```c
VarInfo* var_info = &s->vars[v];
if (var_info->value == TRUE && var_info->level == 0) {
    // Variable v is true at decision level 0 (unit clause)
}
```

---

## Trail (include/solver.h)

### Trail Entry

Stores the assignment history:

```c
typedef struct Trail {
    Lit   lit;    // Assigned literal
    Level level;  // Decision level
} Trail;
```

**Solver trail fields:**
```c
Trail*   trail;        // Assignment stack
uint32_t trail_size;   // Current size (# of assignments)
uint32_t trail_lim;    // Next decision position
uint32_t qhead;        // Propagation queue head
Level*   trail_lims;   // Decision level limits
Level    decision_level; // Current decision level
```

**Invariants:**
- `trail[0..trail_size-1]` contains all assignments
- `trail[0..qhead-1]` have been propagated
- `trail[qhead..trail_size-1]` are pending propagation
- `trail_lims[i]` = start position of level i+1

**Example:**
```c
// At decision level 2, trail might look like:
// [x1=T@0, x2=T@0, x3=F@1, x4=T@1, x5=T@2, x6=F@2, x7=T@2]
//  ^^^^^^^^^^^^^^  ^^^^^^^^^^      ^^^^^^^^^^^^^^^^^^^^^^^^
//     level 0        level 1              level 2
//
// trail_lims[0] = 2 (start of level 1)
// trail_lims[1] = 4 (start of level 2)
```

---

## Clause Storage (include/arena.h)

### Arena Allocator

Compact memory allocator for clauses:

```c
typedef struct Arena {
    uint32_t* data;      // Raw memory (32-bit words)
    uint32_t  size;      // Current size (in words)
    uint32_t  capacity;  // Allocated capacity
    uint32_t  wasted;    // Wasted space from deletions
} Arena;
```

**Benefits:**
- **Cache-friendly**: Clauses stored contiguously
- **No fragmentation**: Uses offset-based references (CRef)
- **Compact**: ~40 bytes per clause vs 80+ with pointers
- **Fast access**: O(1) via CRef offset

### Clause Structure

Clause layout in arena (words are 32-bit):

```
Word 0: Header
  [31:24] = flags (deleted, learnt, glue)
  [23:16] = LBD (Literal Block Distance)
  [15:8]  = activity
  [7:0]   = size (number of literals)

Word 1: Literal 0
Word 2: Literal 1
...
Word N: Literal N-1
```

**Header fields (accessed via macros):**
```c
clause_size(cr)      // Number of literals
clause_lbd(cr)       // Literal Block Distance (quality metric)
clause_activity(cr)  // Activity score for clause reduction
clause_deleted(cr)   // Deleted flag
clause_learnt(cr)    // Learnt vs original clause
clause_glue(cr)      // Glue clause (LBD ≤ 2, never delete)
```

**Access pattern:**
```c
CRef cr = /* some clause reference */;
uint32_t size = clause_size(s->arena, cr);
Lit* lits = clause_lits(s->arena, cr);

for (uint32_t i = 0; i < size; i++) {
    Lit lit = lits[i];
    // Process literal
}
```

---

## Watch Lists (include/watch.h)

### Two-Watched Literals

Each literal maintains a watch list:

```c
typedef struct WatchManager {
    Watch** watches;     // Array of watch lists (one per literal)
    uint32_t capacity;   // Total literals (2 * num_vars + 2)
} WatchManager;

typedef struct Watch {
    CRef     cref;       // Clause reference
    Lit      blocker;    // Blocking literal (optimization)
    Watch*   next;       // Next watch in list
} Watch;
```

**Invariants:**
- Each clause has exactly 2 watched literals (first two in clause)
- Watch lists stored at `watches[lit_index(lit)]`
- Blocker literal used to avoid clause lookups

**Blocking literal optimization:**
When a watched literal becomes false, check the blocker first:
- If blocker is true → clause already satisfied, skip
- If blocker is false → must scan clause for new watch

**Example:**
```c
// Clause: (x1 ∨ x2 ∨ x3)
// Watches: x1 and x2
// If x1 becomes false:
//   - Check blocker (x3)
//   - If x3=true, skip (clause satisfied)
//   - Otherwise, look for new watch or propagate
```

---

## VSIDS Heap (include/solver.h)

### Variable Ordering Heap

Binary max-heap for variable selection:

```c
struct {
    Var*     heap;       // Binary heap of variables
    uint32_t size;       // Current heap size
    double   var_inc;    // Activity increment
    double   var_decay;  // Activity decay factor (0.95)
} order;
```

**Heap invariant:**
```
activity[heap[i]] >= activity[heap[2*i+1]]  (left child)
activity[heap[i]] >= activity[heap[2*i+2]]  (right child)
```

**Operations:**
- `heap_insert(v)`: O(log n) - Insert variable
- `heap_extract_max()`: O(log n) - Get highest activity variable
- `heap_decrease(v)`: O(log n) - Update after activity increase
- `heap_update(v)`: O(log n) - Reposition after activity change

**Activity updates:**
```c
// On conflict involving variable v:
vars[v].activity += var_inc;
heap_decrease(v);  // May need to bubble up

// Periodically rescale to prevent overflow:
if (var_inc > 1e100) {
    for (all variables v) {
        vars[v].activity *= 1e-100;
    }
    var_inc *= 1e-100;
}

// After each conflict:
var_inc *= (1.0 / var_decay);  // Increases future activity bumps
```

---

## Solver Options (include/solver.h)

### SolverOpts Structure

Configuration for solver behavior:

```c
typedef struct SolverOpts {
    // Core parameters
    uint32_t max_conflicts;      // Conflict limit
    uint32_t max_decisions;      // Decision limit
    double   max_time;           // Time limit (seconds)

    // VSIDS parameters
    double   var_decay;          // Activity decay (default: 0.95)
    double   var_inc;            // Activity increment (default: 1.0)

    // Restart parameters
    uint32_t restart_first;      // First restart (default: 100)
    double   restart_inc;        // Restart increment (default: 1.5)
    bool     glucose_restart;    // Glucose adaptive restarts
    bool     luby_restart;       // Luby restart sequence
    uint32_t luby_unit;          // Luby unit size (default: 100)

    // Glucose EMA parameters
    bool     glucose_use_ema;    // EMA vs sliding window
    double   glucose_fast_alpha; // Fast MA decay (default: 0.8)
    double   glucose_slow_alpha; // Slow MA decay (default: 0.9999)

    // Glucose AVG parameters
    uint32_t glucose_window_size; // Window size (default: 50)
    double   glucose_k;           // Threshold (default: 0.8)

    // Phase saving
    bool     phase_saving;       // Enable phase saving
    bool     random_phase;       // Random phase selection
    double   random_phase_prob;  // Random probability (default: 0.01)
    bool     adaptive_random;    // Boost when stuck

    // Clause management
    uint32_t max_lbd;           // Max LBD to keep (default: 30)
    uint32_t glue_lbd;          // Glue clause threshold (default: 2)
    double   reduce_fraction;   // Fraction to keep (default: 0.5)
    uint32_t reduce_interval;   // Conflicts between reductions (default: 2000)

    // Output
    bool     verbose;           // Verbose output
    bool     debug;             // Debug output
    bool     quiet;             // Quiet mode
    bool     stats;             // Print statistics
} SolverOpts;
```

**Default values:**
```c
SolverOpts default_opts(void) {
    return (SolverOpts){
        .max_conflicts = 0,       // Unlimited
        .max_decisions = 0,       // Unlimited
        .max_time = 0.0,          // Unlimited

        .var_decay = 0.95,
        .var_inc = 1.0,

        .restart_first = 100,
        .restart_inc = 1.5,
        .luby_restart = true,     // DEFAULT
        .luby_unit = 100,

        .glucose_use_ema = true,
        .glucose_fast_alpha = 0.8,
        .glucose_slow_alpha = 0.9999,
        .glucose_window_size = 50,
        .glucose_k = 0.8,

        .phase_saving = true,
        .random_phase = true,     // DEFAULT (prevents stuck states)
        .random_phase_prob = 0.01,
        .adaptive_random = true,

        .max_lbd = 30,
        .glue_lbd = 2,
        .reduce_fraction = 0.5,
        .reduce_interval = 2000,

        .stats = true,
    };
}
```

---

## Statistics (include/solver.h)

### Solver Statistics

Performance metrics collected during search:

```c
struct {
    uint64_t decisions;          // Total decisions made
    uint64_t propagations;       // Total propagations
    uint64_t conflicts;          // Total conflicts
    uint64_t restarts;           // Total restarts
    uint64_t reduces;            // Clause database reductions
    uint64_t learned_clauses;    // Total learned clauses
    uint64_t learned_literals;   // Total learned literals
    uint64_t deleted_clauses;    // Clauses deleted
    uint64_t subsumed_clauses;   // On-the-fly subsumption
    uint64_t minimized_literals; // Literals minimized away
    uint64_t blocked_clauses;    // BCE preprocessing
    uint64_t max_lbd;            // Maximum LBD seen
    uint64_t glue_clauses;       // Glue clauses (LBD ≤ 2)
    double   start_time;         // Start time
} stats;
```

**Typical output:**
```
c ========== Statistics ==========
c CPU time          : 0.042 s
c Decisions         : 1234
c Propagations      : 5678
c Conflicts         : 234
c Restarts          : 12
c Learned clauses   : 234
c Learned literals  : 1012
c Deleted clauses   : 0
c Blocked clauses   : 15
c Subsumed clauses  : 45
c Minimized literals: 89
c Glue clauses      : 23
c Max LBD           : 8
```

---

## Memory Layout Example

For a small formula with 5 variables and 3 clauses:

```
solver→vars: [VarInfo × 6]  // Index 0 unused, 1-5 for variables
  [0]: unused
  [1]: {value=TRUE, level=0, reason=0, polarity=true, activity=1.5, ...}
  [2]: {value=FALSE, level=1, reason=2, polarity=false, activity=2.1, ...}
  [3]: {value=UNDEF, level=255, reason=INVALID, polarity=true, activity=0.8, ...}
  [4]: {value=TRUE, level=1, reason=2, polarity=true, activity=1.2, ...}
  [5]: {value=FALSE, level=2, reason=3, polarity=false, activity=0.5, ...}

solver→trail: [Trail × 4]
  [0]: {lit=2  (x1), level=0}  // Unit clause
  [1]: {lit=-4 (¬x2), level=1} // Decision
  [2]: {lit=8  (x4), level=1}  // Implied by clause 2
  [3]: {lit=-10 (¬x5), level=2} // Implied by clause 3

solver→arena: Compact clause storage
  Clause 0: [header][2][4][6]      // (x1 ∨ x2 ∨ x3)
  Clause 1: [header][-4][8]        // (¬x2 ∨ x4)
  Clause 2: [header][-4][-10][6]   // (¬x2 ∨ ¬x5 ∨ x3)

solver→watches: Watch lists
  watches[lit_index(2)]:   → Clause 0
  watches[lit_index(4)]:   → Clause 0
  watches[lit_index(-4)]:  → Clause 1 → Clause 2
  watches[lit_index(8)]:   → Clause 1
  watches[lit_index(-10)]: → Clause 2
  watches[lit_index(6)]:   → Clause 2

solver→order.heap: [3, 1, 4, 5, 2]  // Max-heap by activity
  Heap structure:
       x3 (2.1)
      /        \
    x1(1.5)   x4(1.2)
    /    \
  x5(.8) x2(.5)
```

This compact layout achieves excellent cache locality and minimal memory overhead!
