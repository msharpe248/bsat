# The History of SAT Solving

A timeline of the Boolean Satisfiability Problem from its foundations to modern industrial applications.

## Table of Contents

1. [Pre-History: Foundations of Logic](#pre-history-foundations-of-logic)
2. [The Birth of SAT (1960-1971)](#the-birth-of-sat-1960-1971)
3. [NP-Completeness and Theoretical Foundations (1971-1979)](#np-completeness-and-theoretical-foundations-1971-1979)
4. [The Dark Ages (1980-1995)](#the-dark-ages-1980-1995)
5. [The SAT Revolution (1996-2009)](#the-sat-revolution-1996-2009)
6. [Industrial Era (2010-Present)](#industrial-era-2010-present)
7. [Key People](#key-people)
8. [Major Breakthroughs](#major-breakthroughs)
9. [Timeline at a Glance](#timeline-at-a-glance)

---

## Pre-History: Foundations of Logic

### 1847: Boolean Algebra
**George Boole** publishes "The Mathematical Analysis of Logic"
- Introduces algebraic system for logic (Boolean algebra)
- Variables can be True or False
- Operations: AND (∧), OR (∨), NOT (¬)
- Foundation for all digital computing

**Impact**: Without Boolean algebra, there is no SAT problem!

### 1879: Predicate Logic
**Gottlob Frege** develops predicate logic
- First complete formal system for logic
- Quantifiers: ∀ (for all), ∃ (there exists)
- Foundation for mathematical logic

### 1928: Decision Problem (Entscheidungsproblem)
**David Hilbert** poses the decision problem:
> "Is there an algorithm to determine if a logical statement is valid?"

This would haunt computer science for decades.

### 1936: Undecidability
**Alonzo Church** and **Alan Turing** independently prove:
- The Entscheidungsproblem is undecidable
- Church: Lambda calculus
- Turing: Turing machines
- Some problems cannot be solved by any algorithm

**But**: Satisfiability for propositional logic (SAT) IS decidable!

### 1937: Claude Shannon's Thesis
**Claude Shannon** shows Boolean algebra can design circuits
- Master's thesis: "A Symbolic Analysis of Relay and Switching Circuits"
- Most important master's thesis ever written
- Foundation of digital circuit design

---

## The Birth of SAT (1960-1971)

### 1960: Davis-Putnam Algorithm
**Martin Davis** and **Hilary Putnam** publish first SAT solver

Paper: *"A Computing Procedure for Quantification Theory"*

**Algorithm**:
- Resolution-based
- Eliminates variables one by one
- **Problem**: Exponential space (generates many clauses)

**Significance**: Proved SAT is solvable in practice, not just theory

### 1962: DPLL Algorithm
**Martin Davis**, **George Logemann**, and **Donald Loveland** improve the algorithm

Paper: *"A Machine Program for Theorem-Proving"*

**Key Innovation**: Backtracking search instead of resolution
- Only needs O(n) space (recursion stack)
- Much more practical
- **This is still the foundation of modern solvers!**

**The DPLL Algorithm**:
```
1. Choose a variable
2. Try setting it to True
3. Simplify the formula
4. If satisfied → done!
5. If conflict → backtrack and try False
6. Repeat until done or all options exhausted
```

**Impact**: Made SAT solving feasible for small problems

### 1971: The Thunderbolt - Cook-Levin Theorem

**Stephen Cook** (and independently **Leonid Levin** in USSR) prove:

> **SAT is NP-complete**

Paper: *"The Complexity of Theorem-Proving Procedures"*

**What this means**:
1. SAT is in NP (solutions can be verified in polynomial time)
2. SAT is NP-hard (any NP problem can be reduced to SAT)
3. If SAT has a polynomial-time algorithm, then P = NP
4. Conversely, if P ≠ NP, then SAT requires exponential time

**Revolutionary Impact**:
- SAT becomes the **canonical NP-complete problem**
- Thousands of problems reduced to SAT
- One of the most important results in computer science history
- Cook wins Turing Award in 1982 for this work

**The Big Question**: Is P = NP?
- Still unsolved!
- Clay Mathematics Institute: $1,000,000 prize
- Most computer scientists believe P ≠ NP

---

## NP-Completeness and Theoretical Foundations (1971-1979)

### 1972: Karp's 21 Problems
**Richard Karp** shows 21 classic problems are NP-complete

Paper: *"Reducibility Among Combinatorial Problems"*

Includes:
- 3SAT (SAT with exactly 3 literals per clause)
- Graph coloring
- Clique
- Vertex cover
- Traveling salesman
- Knapsack

**Impact**: Explosion of NP-completeness theory

### 1973: 2SAT is Polynomial-Time
**Krom** and others show 2SAT (2 literals per clause) is in P
- Can be solved in O(n) time
- Uses implication graph
- Strongly connected components

**Key Insight**: There's a phase transition from P (2SAT) to NP-complete (3SAT)

### 1974: Schaefer's Dichotomy Theorem
**Thomas Schaefer** characterizes which SAT variants are polynomial

Paper: *"The Complexity of Satisfiability Problems"*

Shows exactly which constraint types give P vs NP-complete:
- 2SAT: P
- Horn-SAT: P
- XOR-SAT: P
- 3SAT and beyond: NP-complete

**Impact**: Complete understanding of "easy" vs "hard" SAT variants

### 1978: Random 3SAT Phase Transition
Early observations that random 3SAT has a satisfiability phase transition
- Ratio m/n (clauses/variables)
- m/n < 4.26: Usually SAT
- m/n ≈ 4.26: **Hardest instances** (50% SAT)
- m/n > 4.26: Usually UNSAT

**Mystery**: Why is the phase transition the hardest region?

---

## The Dark Ages (1980-1995)

### The Problem: Pessimism

After Cook-Levin theorem, many believe SAT is **intractable**:
- NP-completeness suggests exponential worst case
- DPLL works on small instances but doesn't scale
- Hardware verification and planning problems need SAT solvers
- **General belief**: SAT solvers can't be practical

### 1980s: Incremental Progress

Small improvements to DPLL:
- Better heuristics for choosing variables
- Improved data structures
- Still fundamentally exponential behavior

### 1985: Horn-SAT Solver
Efficient algorithms for Horn clauses (≤1 positive literal):
- O(n+m) time complexity
- Used in logic programming (Prolog)
- Shows specialized solvers can be very efficient

### 1988: Local Search - GSAT
**Bart Selman**, **Hector Levesque**, **David Mitchell** introduce GSAT

Paper: *"A New Method for Solving Hard Satisfiability Problems"*

**Algorithm**: Random walk local search
- Start with random assignment
- Flip variables to satisfy more clauses
- Can escape local minima

**Innovation**: First practical solver for large instances!
- Incomplete (may not find solution)
- But fast when solution exists

### 1992: Watched Literals
**João Marques-Silva** proposes watched literals data structure
- Track only 2 literals per clause
- Efficient unit propagation
- O(1) for most operations

**Impact**: Critical for modern solver performance

### 1994: WalkSAT
**Bart Selman**, **Henry Kautz**, **Bram Cohen** improve GSAT

Paper: *"Noise Strategies for Improving Local Search"*

**Algorithm**: Add randomness to escape local minima
- With probability p: random flip
- With probability 1-p: greedy flip

**Impact**: Even better than GSAT, used in many applications

---

## The SAT Revolution (1996-2009)

### 1996: The Revolution Begins - GRASP

**João Marques-Silva** and **Karem Sakallah** introduce GRASP

Paper: *"GRASP: A Search Algorithm for Propositional Satisfiability"*

**Revolutionary Ideas**:
1. **Conflict-Driven Clause Learning (CDCL)**
   - Learn from conflicts
   - Add "learned clauses" to prevent repeating mistakes
   - Prunes exponentially large search spaces

2. **Non-chronological backtracking**
   - Jump back further when hitting conflicts
   - Not just one step back

3. **Implication graph**
   - Track chains of implications
   - Analyze conflicts to learn the "real reason"

**Impact**:
- 10-100× speedup over DPLL
- Suddenly, large industrial problems become solvable
- The **"SAT revolution"** begins

### 1999: Chaff Solver
**Matthew Moskewicz** et al. at Princeton introduce Chaff

Paper: *"Chaff: Engineering an Efficient SAT Solver"*

**Innovations**:
1. **VSIDS (Variable State Independent Decaying Sum)**
   - Dynamic variable ordering heuristic
   - "Learn from recent conflicts"
   - Adapts to problem structure

2. **Lazy data structures**
   - Watched literals implementation
   - Efficient clause management

**Performance**: Another 10× speedup over GRASP

**Impact**: Sets new standard for solver engineering

### 2001: zChaff
Improved version of Chaff
- Even faster
- Widely used in industry
- Standard baseline for comparisons

### 2002: SAT Competition Begins

First International SAT Competition:
- Compare solvers on standard benchmarks
- Categories: random, industrial, crafted
- Drives innovation through competition

**Impact**:
- Community building
- Standard benchmarks
- Rapid progress (new winner almost every year)
- Continues annually to this day

### 2003-2004: MiniSAT

**Niklas Eén** and **Niklas Sörensson** release MiniSAT

Paper: *"An Extensible SAT-solver"*

**Philosophy**:
- Minimal, clean implementation (< 600 lines of core code!)
- Easy to understand and extend
- Still highly competitive

**Impact**:
- Becomes most influential solver
- Basis for hundreds of research papers
- Standard teaching tool
- Template for new solvers

**MiniSAT is to SAT solving what UNIX is to operating systems!**

### 2005: Bounded Model Checking

**SAT solvers revolutionize hardware verification**:
- Check if circuits satisfy properties
- Find bugs in processor designs
- Intel, IBM, others adopt SAT-based verification

**Result**: SAT solvers prevent hardware bugs worth millions/billions

### 2007: Glucose Solver

**Gilles Audemard** and **Laurent Simon** introduce Glucose

Paper: *"Predicting Learnt Clauses Quality in Modern SAT Solvers"*

**Innovation**: Clause management based on "Literal Block Distance" (LBD)
- Measures clause "quality"
- Aggressive deletion of low-quality learned clauses
- Keeps memory usage low

**Performance**: Dominates SAT competitions 2009-2011

### 2008: CryptoMiniSat

**Mate Soos** creates CryptoMiniSat for cryptography problems

**Innovations**:
- XOR clause handling
- Gaussian elimination integration
- Specialized for cryptographic instances

**Impact**: Used to break weakened cryptographic schemes

### 2009: Parallel SAT Solving

Multi-core solvers emerge:
- Portfolio approach (run multiple strategies)
- Divide-and-conquer parallelism
- Share learned clauses between threads

**Examples**: ManySAT, Plingeling

---

## Industrial Era (2010-Present)

### 2010s: SAT Solvers Go Mainstream

**Applications everywhere**:
- **Hardware**: Chip verification, synthesis
- **Software**: Program verification, test generation
- **AI**: Planning, scheduling, constraint solving
- **Security**: Cryptanalysis, vulnerability detection
- **Bioinformatics**: Protein folding, gene regulation
- **Operations Research**: Timetabling, packing

**Trend**: Problems with millions of variables and clauses now routine

### 2011: Lingeling

**Armin Biere** releases Lingeling
- Wins SAT Competition 2011
- Highly optimized CDCL implementation
- Focus on clause database management

### 2013: Schöning's Algorithm Refinements

**Uwe Schöning's** randomized 3SAT algorithm (1999) gets renewed attention:
- O(1.334^n) expected time
- Provably better than O(2^n)
- Simple random walk approach

**Theoretical impact**: Shows randomization helps even for worst-case

### 2014: Cube-and-Conquer

**Armin Biere** et al. introduce parallel SAT solving strategy

Paper: *"Cube and Conquer: Guiding CDCL SAT Solvers by Lookaheads"*

**Method**:
- Phase 1: "Cube" - divide into subproblems (lookahead)
- Phase 2: "Conquer" - solve each with CDCL (parallel)

**Achievement**: Solves previously intractable problems
- Schur number 5
- Van der Waerden numbers
- Pythagorean triples problem

### 2016: Pythagorean Triples Solved

**Marijn Heule** et al. solve 30-year-old problem using SAT

Paper: *"Solving and Verifying the Boolean Pythagorean Triples Problem via Cube-and-Conquer"*

**Problem**: Can you 2-color {1,2,...,7825} without monochromatic Pythagorean triple (a²+b²=c²)?

**Answer**: No! But 7824 is possible.

**Solution**:
- 200 TB proof (largest mathematical proof ever)
- 2 days on supercomputer with 800 cores
- 4×10^28 decisions explored

**Impact**: SAT solvers can solve open mathematical problems!

### 2017: MaxSAT and Optimization

Focus shifts to **MaxSAT** (optimization):
- Instead of "Is formula satisfiable?"
- Ask "What's the maximum number of clauses we can satisfy?"

**Applications**: More natural for real problems (soft constraints)

### 2018: Machine Learning Meets SAT

**Neural networks** start influencing SAT solving:
- Learn good variable orderings from past instances
- Predict clause relevance
- Early stage but promising

**Examples**: NeuroSAT, DeepSAT

### 2019: Kissat

**Armin Biere** releases Kissat
- Won SAT Competition 2019-2020
- "Keep It Simple SAT"
- Builds on decades of incremental improvements
- Clean, maintainable code

### 2020: Cloud SAT Solving

SAT solving moves to cloud infrastructure:
- Massive parallelism (1000+ cores)
- Solve instances in minutes that would take years on desktop
- SAT-as-a-Service offerings

### 2021: Handbook of Satisfiability (2nd Edition)

**Armin Biere** et al. publish 1500-page updated handbook
- Comprehensive survey of SAT solving
- Theory, algorithms, applications
- Definitive reference

### 2022-Present: Current Frontiers

**Active research areas**:

1. **Portfolio and Parallel Solvers**
   - Optimal strategy selection
   - Efficient clause sharing

2. **SAT+ML Integration**
   - Learning from problem distributions
   - Adaptive heuristics

3. **Proof Complexity**
   - Understanding why CDCL works so well
   - Lower bounds and limits

4. **Specialized Solvers**
   - XOR-SAT (cryptography)
   - Cardinality constraints
   - Pseudo-Boolean constraints

5. **Incremental and Assumptions**
   - Reuse work between similar instances
   - Interactive solving

6. **Quantum SAT Solving**
   - Can quantum computers help?
   - Early theoretical work

---

## Key People

### Pioneers (1960s-1970s)

**Martin Davis** (1928-2023)
- Davis-Putnam algorithm (1960)
- DPLL algorithm (1962)
- Mathematical logic pioneer

**Hilary Putnam** (1926-2016)
- Davis-Putnam algorithm
- Philosopher and logician

**Stephen Cook** (1939-)
- Cook-Levin theorem (1971)
- Turing Award 1982
- Professor at University of Toronto

**Richard Karp** (1935-)
- 21 NP-complete problems (1972)
- Turing Award 1985

### SAT Revolution Era (1990s-2000s)

**Bart Selman** (1954-)
- GSAT (1988)
- WalkSAT (1994)
- Local search pioneer

**João Marques-Silva** (1965-)
- GRASP (1996)
- CDCL pioneer
- Professor at IRIT, France

**Sharad Malik** (1960-)
- GRASP (1996)
- Hardware verification applications

**Niklas Eén** (1976-)
- MiniSAT (2003)
- Minimal, influential design
- Now at Google

**Niklas Sörensson** (1978-)
- MiniSAT (2003)
- Co-creator with Eén

### Modern Era (2010s+)

**Armin Biere** (1967-)
- Lingeling, Plingeling, Treengeling
- Cube-and-Conquer
- Professor at University of Freiburg
- **The most prolific SAT solver developer**

**Marijn Heule** (1979-)
- DRAT proof checking
- Pythagorean triples solution
- Professor at Carnegie Mellon

**Mate Soos** (1986-)
- CryptoMiniSat
- XOR handling innovations

**Gilles Audemard** (1974-)
- Glucose solver
- Clause learning improvements

**Laurent Simon** (1972-)
- Glucose solver
- Restart strategies

---

## Major Breakthroughs

### Algorithmic Breakthroughs

1. **DPLL (1962)** - Backtracking search, O(n) space
2. **2SAT in P (1973)** - Strongly connected components
3. **CDCL (1996)** - Learn from conflicts
4. **VSIDS (1999)** - Adaptive variable ordering
5. **Watched Literals (1999)** - Efficient propagation
6. **Clause Learning Management (2007)** - LBD/Glucose heuristics
7. **Cube-and-Conquer (2014)** - Massive parallelism

### Theoretical Breakthroughs

1. **Cook-Levin Theorem (1971)** - SAT is NP-complete
2. **Schaefer's Dichotomy (1974)** - P vs NP-complete variants
3. **Phase Transition (1992-1996)** - Understanding hardness
4. **Resolution Complexity (2000s)** - Lower bounds on proof size
5. **CDCL Analysis (2010s)** - Why learning works so well

### Application Breakthroughs

1. **Hardware Verification (2000s)** - Find bugs in chips
2. **Software Verification (2000s)** - Prove program correctness
3. **Planning (2000s)** - AI planning via SAT
4. **Bioinformatics (2010s)** - Protein folding, pathway analysis
5. **Mathematical Discovery (2016)** - Pythagorean triples

---

## Timeline at a Glance

### Pre-1960: Foundations
- 1847: Boolean algebra (Boole)
- 1936: Computability theory (Church, Turing)
- 1937: Boolean circuits (Shannon)

### 1960-1971: Birth of SAT
- 1960: Davis-Putnam algorithm
- 1962: **DPLL algorithm**
- 1971: **SAT is NP-complete** (Cook-Levin)

### 1971-1979: Theory Golden Age
- 1972: 21 NP-complete problems (Karp)
- 1973: 2SAT in polynomial time
- 1974: Schaefer's dichotomy

### 1980-1995: Dark Ages
- 1988: GSAT (local search)
- 1992: Watched literals idea
- 1994: WalkSAT
- General pessimism about practical SAT solving

### 1996-2009: SAT Revolution
- 1996: **GRASP - CDCL invented**
- 1999: **Chaff - VSIDS heuristic**
- 2002: First SAT Competition
- 2003: **MiniSAT released**
- 2005: Bounded model checking adoption
- 2007: Glucose solver

### 2010-Present: Industrial Era
- 2011: Lingeling dominates
- 2014: Cube-and-Conquer
- 2016: **Pythagorean triples solved** (200 TB proof)
- 2019: Kissat
- 2021: Handbook of Satisfiability 2nd ed
- 2023: ML+SAT integration, cloud solving

---

## Why Did SAT Solving Succeed?

Despite being NP-complete, SAT solvers work in practice because:

### 1. Real Problems Have Structure
- Not random
- Locality, modularity
- CDCL exploits structure through learning

### 2. Exponential Space Is Huge
- 2^100 is impossibly large
- But 2^30 is feasible (~1 billion)
- Problems with structure: effective branching factor ≪ 2

### 3. Learned Clauses Prune Search
- One learned clause can eliminate exponentially many branches
- Learning from conflicts is incredibly powerful

### 4. Decades of Engineering
- Watched literals: O(1) operations
- Efficient data structures
- Cache-friendly memory access
- Incremental solving

### 5. Competition-Driven Innovation
- Annual SAT competitions since 2002
- Public benchmarks
- Open-source solvers
- Rapid iteration and improvement

---

## The Remarkable Trajectory

From 1971 to 2023:

**1971**: SAT is NP-complete
→ "This problem is intractable"
→ Worst-case exponential

**1996**: CDCL invented
→ "Wait, we can solve large instances!"

**2023**: Routine industrial use
→ Millions of variables
→ Verify chip designs
→ Solve open math problems
→ Powers critical infrastructure

**The Lesson**: Worst-case complexity doesn't always matter. Real problems have structure. Clever algorithms exploit that structure.

---

## The Future

### Next 10 Years: Predictions

1. **SAT+ML Integration**: Neural networks guide search
2. **Quantum SAT?**: Can quantum computers help? (Unknown!)
3. **Distributed Cloud Solving**: 1000+ core solving as standard
4. **Automated Algorithm Selection**: AI picks best solver per instance
5. **New Applications**: Drug discovery, climate modeling, scientific computing
6. **Proof Verification**: Formal verification of SAT solver correctness
7. **MaxSAT Dominance**: Optimization becomes more common than decision

### The Unsolved Mystery: P vs NP

If P ≠ NP (likely):
- SAT requires exponential time in worst case
- But practical instances remain solvable
- We continue improving heuristics

If P = NP (unlikely!):
- Revolution in computing
- Polynomial SAT algorithm exists
- Would transform cryptography, optimization, AI

**Current odds** (informal poll of experts):
- P ≠ NP: ~98% believe this
- P = NP: ~2%

But we still don't have a proof either way!

---

## Resources

### Key Papers

1. **Cook (1971)**: "The Complexity of Theorem-Proving Procedures"
2. **Davis et al. (1962)**: "A Machine Program for Theorem-Proving"
3. **Marques-Silva & Sakallah (1996)**: "GRASP—A New Search Algorithm for Satisfiability"
4. **Moskewicz et al. (2001)**: "Chaff: Engineering an Efficient SAT Solver"
5. **Eén & Sörensson (2003)**: "An Extensible SAT-solver"

### Books

1. **Handbook of Satisfiability (2021)** - Comprehensive 1500-page reference
2. **The Art of Computer Programming Vol 4B (Knuth, 2022)** - Includes SAT solving
3. **Decision Procedures (Kroening & Strichman, 2016)** - Textbook

### Online

- [SAT Competition](http://www.satcompetition.org/) - Annual competition
- [SATLIB](https://www.cs.ubc.ca/~hoos/SATLIB/) - Benchmark library
- [MiniSAT](http://minisat.se/) - Reference implementation
- [SAT Heritage](http://www.satlive.org/) - Community resources

---

## Conclusion

The history of SAT solving is one of the great success stories in computer science:

- Started with theoretical algorithms (1960s)
- Deemed intractable after NP-completeness (1971)
- Languished in "dark ages" (1980s-early 1990s)
- **Revolutionary breakthrough** with CDCL (1996)
- Explosive growth in capability (1996-present)
- Now solves problems with millions of variables routinely

**Key Lesson**:

> "NP-complete" doesn't mean "impossible in practice"

Real-world problems have structure. Clever algorithms exploit that structure. The result: exponential worst-case, but often linear-like behavior on real instances.

SAT solving transformed from academic curiosity to industrial workhorse in just 30 years. It now underpins hardware verification, software testing, AI planning, and has even solved open mathematical problems.

**The journey continues!**

---

*For more on SAT solving, see:*
- [Introduction to SAT](introduction.md)
- [Theory & References](theory.md)
- [Reading List](reading-list.md)
- [DPLL Solver](dpll-solver.md)
