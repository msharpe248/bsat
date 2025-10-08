# Theory & References

Academic foundations, important papers, and further reading for SAT solving.

## Table of Contents

1. [Foundational Papers](#foundational-papers)
2. [Complexity Theory](#complexity-theory)
3. [Modern Developments](#modern-developments)
4. [Textbooks](#textbooks)
5. [Online Courses](#online-courses)
6. [Tools and Benchmarks](#tools-and-benchmarks)

## Foundational Papers

### The Origins (1960s)

#### Davis & Putnam (1960)
**"A Computing Procedure for Quantification Theory"**
- First practical SAT solving algorithm
- Used resolution and variable elimination
- Exponential space complexity
- [ACM Link (paywalled)](https://dl.acm.org/doi/10.1145/321033.321034)

**Key Contribution**: Showed that systematic search could solve SAT

#### Davis, Logemann & Loveland (1962)
**"A Machine Program for Theorem-Proving"**
- Improved Davis-Putnam with backtracking
- Linear space (recursion stack only)
- Foundation for modern SAT solvers
- [ACM Link (paywalled)](https://dl.acm.org/doi/10.1145/368273.368557)

**Key Contribution**: The DPLL algorithm we still use today

### Complexity Theory (1970s)

#### Cook (1971)
**"The Complexity of Theorem-Proving Procedures"**
- Proved SAT is NP-complete
- First NP-completeness proof ever
- Founded computational complexity theory
- [ACM Link (paywalled)](https://dl.acm.org/doi/10.1145/800157.805047)
- [Free PDF](https://www.cs.toronto.edu/~sacook/homepage/1971.pdf)

**Key Contribution**: Showed SAT is the "hardest" problem in NP

#### Levin (1973)
**Independent NP-completeness proof**
- Proved NP-completeness independently (in Russian)
- Hence "Cook-Levin theorem"

#### Karp (1972)
**"Reducibility Among Combinatorial Problems"**
- Showed 21 problems are NP-complete
- Included 3SAT, graph coloring, Hamiltonian path, etc.
- Established reduction technique
- [Link](https://link.springer.com/chapter/10.1007/978-1-4684-2001-2_9)

**Key Contribution**: Connected SAT to hundreds of practical problems

### The Phase Transition (1990s)

#### Mitchell, Selman & Levesque (1992)
**"Hard and Easy Distributions of SAT Problems"**
- Discovered satisfiability phase transition
- Random 3SAT has sharp threshold at m/n ≈ 4.26
- Hardest instances are at the threshold
- [Link](https://www.aaai.org/Papers/AAAI/1992/AAAI92-071.pdf)

**Key Contribution**: Explained why some random instances are extremely hard

#### Monasson et al. (1999)
**"Determining Computational Complexity from Characteristic 'Phase Transitions'"**
- Physics approach to SAT
- Statistical mechanics analysis
- [Nature paper](https://www.nature.com/articles/20506)

**Key Contribution**: Connection between SAT and statistical physics

## Complexity Theory

### P vs NP

The **Millennium Prize Problem** (Clay Mathematics Institute, $1 million):

**Question**: Does P = NP?

- **P**: Problems solvable in polynomial time
  - Examples: sorting, shortest path, 2SAT

- **NP**: Problems where solutions are verifiable in polynomial time
  - Examples: SAT, graph coloring, TSP

- **NP-complete**: Hardest problems in NP
  - SAT, 3SAT, graph 3-coloring, etc.
  - If ANY NP-complete problem has polynomial algorithm, then P = NP!

**Current Status**: Unknown (most believe P ≠ NP)

**Why It Matters**:
- If P = NP: Cryptography breaks, optimization becomes easy
- If P ≠ NP: Some problems are fundamentally hard

**Further Reading**:
- [Clay Mathematics Institute](https://www.claymath.org/millennium-problems/p-vs-np-problem)
- [Scott Aaronson: P vs NP](https://www.scottaaronson.com/blog/?p=122)
- [Fortnow: The Status of P vs NP (2009)](https://dl.acm.org/doi/10.1145/1562164.1562186)

### Complexity Classes

```
P ⊆ NP ⊆ PSPACE ⊆ EXP

Where:
- P: Polynomial time
- NP: Nondeterministic polynomial time
- PSPACE: Polynomial space
- EXP: Exponential time
```

**SAT Variants**:
- **2SAT**: P (polynomial time, O(n+m))
- **3SAT**: NP-complete
- **QSAT** (Quantified SAT): PSPACE-complete
- **#SAT** (Count solutions): #P-complete

### Reductions

Many problems reduce to SAT:

```
Problem X ≤ₚ SAT

Meaning: If we can solve SAT efficiently, we can solve X efficiently
```

**Examples**:
- Graph 3-Coloring ≤ₚ 3SAT
- Hamiltonian Path ≤ₚ SAT
- Subset Sum ≤ₚ SAT
- Factoring ≤ₚ SAT (inefficiently)

**Practical Impact**: SAT solvers are general-purpose problem solvers!

## Modern Developments

### CDCL (1990s-2000s)

#### Marques-Silva & Sakallah (1996)
**"GRASP: A New Search Algorithm for Satisfiability"**
- Introduced conflict-driven clause learning
- Non-chronological backtracking
- Foundation for modern solvers
- [Link](https://doi.org/10.1109/ICCAD.1996.569607)

#### Moskewicz et al. (2001)
**"Chaff: Engineering an Efficient SAT Solver"**
- VSIDS variable ordering heuristic
- Watched literals data structure
- Made SAT solving practical for large instances
- [Link](https://dl.acm.org/doi/10.1145/378239.379017)

#### Eén & Sörensson (2003)
**"An Extensible SAT-solver" (MiniSat)**
- Clean, efficient implementation
- Open source, widely used
- Basis for many modern solvers
- [Link](http://minisat.se/Papers.html)

**Key Contribution**: Made CDCL solvers 1000x faster than DPLL

### Preprocessing and Inprocessing (2000s-2010s)

#### Eén & Biere (2005)
**"Effective Preprocessing in SAT Through Variable and Clause Elimination"**
- SatELite preprocessor
- Variable elimination, subsumption
- [Link](https://link.springer.com/chapter/10.1007/11527695_4)

### Portfolio and Parallel Solvers (2010s)

#### Audemard & Simon (2009)
**"Predicting Learnt Clauses Quality in Modern SAT Solvers" (Glucose)**
- LBD (Literal Block Distance) metric
- Aggressive clause deletion
- Won multiple SAT competitions
- [Link](https://www.ijcai.org/Proceedings/09/Papers/074.pdf)

#### Balint & Schöning (2012)
**"Choosing Probability Distributions for Stochastic Local Search"**
- Improved WalkSAT variants
- Probability distribution selection

### Recent Developments (2020s)

#### ML and SAT (2018-present)
- **Neural network guided solving**: Learning variable selection
- **GNN for SAT**: Graph neural networks
- **AlphaZero-style search**: Combining learning and search

**Papers**:
- Selsam et al. (2019): "Learning a SAT Solver from Single-Bit Supervision"
- Kurin et al. (2020): "Can Q-Learning with Graph Networks Learn a Generalizable Branching Heuristic for a SAT Solver?"

## Textbooks

### Comprehensive References

#### Handbook of Satisfiability (2021, 2nd edition)
- Editors: Biere, Heule, van Maaren, Walsh
- 1500+ pages covering all aspects of SAT
- **The** reference book for SAT
- [IOS Press](https://www.iospress.com/catalog/books/handbook-of-satisfiability-2)

**Chapters of Interest**:
- Ch 4: DPLL and CDCL
- Ch 5: Conflict-Driven Clause Learning
- Ch 6: Look-Ahead Based SAT Solvers

### Algorithm Textbooks

#### The Art of Computer Programming, Vol 4B (Knuth, 2015)
- Section 7.2.2.2: Satisfiability
- Knuth's perspective on SAT algorithms
- Exercises are famously challenging

#### Decision Procedures (Kroening & Strichman, 2016)
- Ch 2: Propositional Logic
- Covers SAT solving and applications
- Practical engineering perspective

#### Introduction to Algorithms (CLRS, 4th ed, 2022)
- Ch 34: NP-Completeness
- Background on complexity theory
- Classic algorithms textbook

### Introductory Books

#### "SAT/SMT by Example" (Yurichev, online)
- Practical examples and applications
- Free online book
- Great for beginners
- [Link](https://sat-smt.codes/)

## Online Courses

### MOOCs and Lectures

#### Coursera: "Discrete Optimization"
- University of Melbourne
- Includes SAT solving module
- [Link](https://www.coursera.org/learn/discrete-optimization)

#### MIT OCW: "Introduction to Algorithms"
- Covers NP-completeness and SAT
- Lecture videos and notes
- [Link](https://ocw.mit.edu/courses/6-006-introduction-to-algorithms-spring-2020/)

### Video Lectures

- **SAT/SMT Summer School**: Annual event with recorded lectures
  - [2021 Videos](https://sat-smt-ws.gitlab.io/2021/)

- **Armin Biere's Lectures**: Author of many SAT solvers
  - Available on YouTube

## Tools and Benchmarks

### SAT Solvers

#### Industrial Strength
- **MiniSat**: Classic CDCL solver, widely used
  - [Website](http://minisat.se/)
- **Glucose**: Modern CDCL with LBD
  - [Website](https://www.labri.fr/perso/lsimon/glucose/)
- **CryptoMiniSat**: Optimized for cryptographic problems
  - [GitHub](https://github.com/msoos/cryptominisat)
- **Lingeling**: High-performance solver
  - [Website](http://fmv.jku.at/lingeling/)

#### Stochastic Solvers
- **WalkSAT**: Local search algorithm
- **ProbSAT**: Probability-based local search

#### Portfolio Solvers
- **SATzilla**: Machine learning portfolio
- **ManySAT**: Parallel portfolio solver

### Benchmarks

#### SATLIB
- Collection of benchmark SAT instances
- Random 3SAT, graph coloring, etc.
- [Website](https://www.cs.ubc.ca/~hoos/SATLIB/benchm.html)

#### SAT Competition
- Annual competition since 2002
- Thousands of real-world instances
- Tracks: Random, Application, Hard Combinatorial
- [Website](http://satcompetition.org/)

#### DIMACS Format
- Standard file format for CNF
- Used by all modern solvers
- [Format specification](http://www.satcompetition.org/2009/format-benchmarks2009.html)

Example DIMACS file:
```
c This is a comment
p cnf 3 2
1 2 0
-1 3 0
```
- `p cnf 3 2`: 3 variables, 2 clauses
- `1 2 0`: Clause (x₁ ∨ x₂)
- `-1 3 0`: Clause (¬x₁ ∨ x₃)

### Verification Tools

#### DRAT Proofs
- **DRAT**: Deletion and Resolution Asymmetric Tautologies
- Standard format for unsatisfiability proofs
- Allows verification of UNSAT results

#### Certified UNSAT
- **drat-trim**: Verify DRAT proofs
- **GRIT**: Proof checker

### Visualization Tools

#### SAT Solver Visualizations
- [DPLL Visualizer](https://cse442-17f.github.io/Conflict-Driven-Clause-Learning/)
- [Resolution Proof Visualizer](http://www.cril.univ-artois.fr/KC/proofvisualizer.html)

## Important Surveys and Tutorials

### Survey Papers

#### Malik & Zhang (2009)
**"Boolean Satisfiability: From Theoretical Hardness to Practical Success"**
- Excellent overview of SAT solving
- Evolution from DPLL to CDCL
- [Link](https://www.cs.princeton.edu/~sharad/p394-malik.pdf)

#### Biere et al. (2009)
**"Conflict-Driven Clause Learning SAT Solvers"**
- Comprehensive CDCL tutorial
- Chapter in Handbook of Satisfiability

#### Gomes et al. (2008)
**"Satisfiability Solvers"**
- AI perspective on SAT
- [Link](https://www.cs.cornell.edu/gomes/pdf/2008_gomes_knowledge_satisfiability.pdf)

### Application-Focused

#### Bounded Model Checking
- Biere et al. (2003): "Bounded Model Checking"
- SAT for hardware verification

#### Planning
- Kautz & Selman (1992): "Planning as Satisfiability"
- Encoding planning problems as SAT

#### Cryptanalysis
- Massacci & Marraro (2000): "Logical Cryptanalysis as a SAT Problem"

## Online Communities

### Forums and Discussion
- **SAT Association**: [satassociation.org](http://www.satassociation.org/)
- **Reddit r/algorithms**: Discussions on SAT and complexity
- **MathOverflow**: Theoretical questions
- **StackOverflow**: Implementation questions

### Mailing Lists
- SAT Live: News and announcements

### Conferences
- **SAT**: Annual conference (International Conference on Theory and Applications of Satisfiability Testing)
- **IJCAR**: International Joint Conference on Automated Reasoning
- **TACAS**: Tools and Algorithms for Construction and Analysis of Systems

## Open Problems

### Theoretical
1. **P vs NP**: The big one!
2. **Proof complexity**: Lower bounds for proof systems
3. **Optimal CDCL**: Is there a "best" CDCL implementation?

### Practical
1. **Better heuristics**: Variable ordering, restart strategies
2. **Parallel SAT**: Effective parallelization
3. **Incremental SAT**: Efficiently solve related formulas
4. **ML for SAT**: Can neural networks learn good heuristics?

## Historical Timeline

```
1960  Davis-Putnam algorithm
1962  DPLL algorithm
1971  Cook's NP-completeness proof
1972  Karp's 21 NP-complete problems
1992  Phase transition discovery
1996  GRASP (first CDCL)
2001  Chaff (VSIDS, watched literals)
2003  MiniSat
2009  Glucose
2013  Lingeling
2020+ ML-guided SAT solving
```

## Further Exploration

### For Beginners
1. Start with [Introduction to SAT](introduction.md)
2. Read Malik & Zhang (2009) survey
3. Try implementing basic DPLL
4. Use MiniSat on small instances

### For Advanced Study
1. Read Handbook of Satisfiability
2. Implement CDCL solver
3. Read SAT competition papers
4. Experiment with benchmarks

### For Researchers
1. Follow SAT conferences
2. Read recent papers on ML + SAT
3. Study proof complexity
4. Contribute to open-source solvers

## Conclusion

SAT solving combines:
- **Deep theory**: NP-completeness, proof complexity
- **Engineering**: Efficient data structures, heuristics
- **Applications**: Verification, planning, cryptography
- **Open problems**: P vs NP, better algorithms

The field continues to evolve with new techniques emerging constantly!

---

**Next Steps**:
- Return to [documentation home](README.md)
- Try the [examples and tutorials](examples.md)
- Implement your own solver using [DPLL](dpll-solver.md) or [2SAT](2sat-solver.md)

Happy learning! 📚
