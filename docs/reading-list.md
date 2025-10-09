# SAT Solving: Reading List

A comprehensive collection of books, papers, websites, and resources for learning about Boolean Satisfiability (SAT) solving.

## Table of Contents

1. [Books](#books)
2. [Seminal Papers](#seminal-papers)
3. [Survey Papers & Tutorials](#survey-papers--tutorials)
4. [Algorithm-Specific Papers](#algorithm-specific-papers)
5. [Online Courses & Lectures](#online-courses--lectures)
6. [Websites & Tools](#websites--tools)
7. [Competitions & Benchmarks](#competitions--benchmarks)
8. [Related Topics](#related-topics)

---

## Books

### Comprehensive References

**[Handbook of Satisfiability](https://www.iospress.com/catalog/books/handbook-of-satisfiability-2)** (2nd Edition, 2021)
Edited by Armin Biere, Marijn Heule, Hans van Maaren, Toby Walsh
*The definitive reference for SAT solving. 1500+ pages covering theory, algorithms, and applications.*

- **Chapter 1**: Introduction and Historical Perspective
- **Chapter 4**: Conflict-Driven Clause Learning (CDCL)
- **Chapter 6**: Look-Ahead SAT Solvers
- **Chapter 12**: Applications in Hardware Verification
- **Chapter 13**: Applications in Software Verification

**[The Art of Computer Programming, Volume 4, Fascicle 6: Satisfiability](https://www.amazon.com/Art-Computer-Programming-Fascicle-Satisfiability/dp/0134397606)**
Donald E. Knuth (2015)
*Knuth's comprehensive treatment of SAT with detailed algorithm analysis and exercises.*

- Covers both classic and modern algorithms
- Extensive exercises with difficulty ratings
- Historical perspective from the master algorithmist

**[Decision Procedures: An Algorithmic Point of View](https://www.decision-procedures.org/)** (2nd Edition, 2016)
Daniel Kroening and Ofer Strichman
*Covers SAT and SMT solving with a focus on practical applications.*

- DPLL and CDCL algorithms
- Theory propagation
- SMT (Satisfiability Modulo Theories)
- Applications in verification

### Textbooks

**[Logic for Computer Science: Foundations of Automatic Theorem Proving](https://www.amazon.com/Logic-Computer-Science-Foundations-Automatic/dp/0486780821)** (2nd Edition, 2001)
Jean H. Gallier
*Strong theoretical foundations including resolution and SAT.*

**[Computational Complexity: A Modern Approach](https://theory.cs.princeton.edu/complexity/)** (2009)
Sanjeev Arora and Boaz Barak
*Excellent coverage of NP-completeness, Cook-Levin theorem, and complexity theory.*

**[Introduction to the Theory of Computation](https://www.amazon.com/Introduction-Theory-Computation-Michael-Sipser/dp/113318779X)** (3rd Edition, 2012)
Michael Sipser
*Classic textbook covering P, NP, NP-completeness with clear proofs.*

---

## Seminal Papers

### Foundational Theory

**[The Complexity of Theorem-Proving Procedures](https://dl.acm.org/doi/10.1145/800157.805047)** (1971)
Stephen A. Cook
*The Cook-Levin theorem: SAT is NP-complete.*
```
@inproceedings{Cook1971,
  title={The complexity of theorem-proving procedures},
  author={Cook, Stephen A.},
  booktitle={STOC '71},
  year={1971}
}
```

**[Reducibility Among Combinatorial Problems](https://link.springer.com/chapter/10.1007/978-1-4684-2001-2_9)** (1972)
Richard M. Karp
*21 NP-complete problems including 3-SAT.*

### Classic Algorithms

**[A Computing Procedure for Quantification Theory](https://dl.acm.org/doi/10.1145/368273.368557)** (1960)
Martin Davis and Hilary Putnam
*The original DP (Davis-Putnam) algorithm.*

**[A Machine Program for Theorem-Proving](https://dl.acm.org/doi/10.1145/368273.368557)** (1962)
Martin Davis, George Logemann, and Donald Loveland
*The DPLL algorithm - foundation of modern SAT solvers.*

**[Linear-Time Algorithms for Testing the Satisfiability of Propositional Horn Formulae](https://www.sciencedirect.com/science/article/pii/0743106684900140)** (1984)
William F. Dowling and Jean H. Gallier
*Polynomial-time Horn-SAT solver.*

**[A Linear-Time Algorithm for Testing the Truth of Certain Quantified Boolean Formulas](https://www.sciencedirect.com/science/article/pii/0020019079900024)** (1979)
Bengt Aspvall, Michael F. Plass, and Robert E. Tarjan
*2-SAT solved in linear time using strongly connected components.*

### Modern SAT Solvers

**[GRASP: A Search Algorithm for Propositional Satisfiability](https://ieeexplore.ieee.org/document/569607)** (1996)
João P. Marques-Silva and Karem A. Sakallah
*First CDCL solver - introduced conflict analysis and clause learning.*

**[Chaff: Engineering an Efficient SAT Solver](https://dl.acm.org/doi/10.1145/378239.379017)** (2001)
Matthew W. Moskewicz, Conor F. Madigan, Ying Zhao, Lintao Zhang, Sharad Malik
*Revolutionary optimizations: VSIDS heuristic and watched literals.*

**[An Extensible SAT-solver](https://link.springer.com/chapter/10.1007/978-3-540-24605-3_37)** (2003)
Niklas Eén and Niklas Sörensson
*MiniSat - clean, minimal implementation that became the basis for many modern solvers.*

**[Towards a Better Understanding of the Functionality of a Conflict-Driven SAT Solver](https://link.springer.com/chapter/10.1007/978-3-540-45199-7_4)** (2005)
Niklas Eén and Niklas Sörensson
*Detailed explanation of modern CDCL architecture.*

---

## Survey Papers & Tutorials

**[SAT Solvers: A Tutorial](https://link.springer.com/chapter/10.1007/978-3-319-10575-8_5)** (2014)
Joao Marques-Silva
*Excellent modern tutorial covering DPLL, CDCL, and applications.*

**[Conflict-Driven Clause Learning SAT Solvers](https://www.cs.princeton.edu/~zkincaid/courses/fall18/readings/SATHandbook-CDCL.pdf)** (2009)
Joao Marques-Silva, Ines Lynce, Sharad Malik
*Comprehensive CDCL tutorial from the Handbook of Satisfiability.*

**[The Quest for Efficient Boolean Satisfiability Solvers](https://link.springer.com/chapter/10.1007/3-540-45620-1_23)** (2002)
Sharad Malik and Lintao Zhang
*Historical perspective on SAT solver evolution.*

**[SAT-Based Methods and Applications in Electronic Design Automation](https://ieeexplore.ieee.org/document/1453456)** (2005)
Mukul R. Prasad, Armin Biere, Aarti Gupta
*Applications of SAT in hardware verification.*

---

## Algorithm-Specific Papers

### CDCL Components

**[Using CSP Look-Back Techniques to Solve Real-World SAT Instances](https://www.aaai.org/Papers/AAAI/1997/AAAI97-046.pdf)** (1997)
Roberto J. Bayardo Jr. and Robert C. Schrag
*Non-chronological backtracking and intelligent backtracking.*

**[Understanding VSIDS Branching Heuristics in Conflict-Driven Clause-Learning SAT Solvers](https://link.springer.com/chapter/10.1007/978-3-319-24318-4_4)** (2015)
Jia Hui Liang, Vijay Ganesh, Ed Zulkoski, Atulan Zaman, Krzysztof Czarnecki
*Deep analysis of the VSIDS heuristic.*

**[Clause Learning in SAT](https://link.springer.com/chapter/10.1007/978-3-642-02777-2_5)** (2009)
Armin Biere
*Comprehensive coverage of clause learning techniques.*

### Preprocessing & Simplification

**[Effective Preprocessing in SAT Through Variable and Clause Elimination](https://link.springer.com/chapter/10.1007/11527695_4)** (2005)
Niklas Eén and Armin Biere
*SatELite preprocessor - variable elimination and subsumption.*

**[Inprocessing Rules](https://link.springer.com/chapter/10.1007/978-3-642-39071-5_14)** (2013)
Matti Järvisalo, Marijn J.H. Heule, Armin Biere
*Simplification techniques during search.*

### Local Search

**[GSAT: A New Method for Solving Hard Satisfiability Problems](https://www.ijcai.org/Proceedings/93-1/Papers/051.pdf)** (1993)
Bart Selman, Hector Levesque, David Mitchell
*Greedy local search for SAT.*

**[Evidence for Invariants in Local Search](https://www.aaai.org/Papers/AAAI/1997/AAAI97-071.pdf)** (1997)
Ian P. Gent and Toby Walsh
*WalkSAT and randomized local search.*

**[Noise Strategies for Improving Local Search](https://www.aaai.org/Papers/AAAI/1994/AAAI94-109.pdf)** (1994)
Bart Selman, Henry A. Kautz, Bram Cohen
*Noise parameter in WalkSAT.*

### Random SAT & Phase Transitions

**[Random k-SAT: Two Moments Suffice to Cross a Sharp Threshold](https://epubs.siam.org/doi/10.1137/S0097539703434231)** (2006)
Dimitris Achlioptas, Assaf Naor, Yuval Peres
*Analysis of the phase transition in random k-SAT.*

**[Critical Behavior in the Satisfiability of Random Boolean Expressions](https://www.science.org/doi/10.1126/science.264.5163.1297)** (1994)
David Mitchell, Bart Selman, Hector Levesque
*Discovery of the phase transition phenomenon.*

**[The Satisfiability Threshold Conjecture: Techniques Behind Upper Bound Improvements](https://link.springer.com/chapter/10.1007/978-3-319-24318-4_2)** (2015)
Jian Ding, Allan Sly, Nike Sun
*Recent progress on understanding the SAT/UNSAT threshold.*

### Parallel & Portfolio Solvers

**[PaInleSS: A Framework for Parallel SAT Solving](https://link.springer.com/chapter/10.1007/978-3-319-66158-2_42)** (2017)
Ludovic Le Frioux, Souheib Baarir, Julien Sopena, Fabrice Kordon
*Framework for parallel SAT solving.*

**[ManySAT: A Parallel SAT Solver](https://jsat.ewi.tudelft.nl/content/volume6/JSAT6_4_Hamadi.pdf)** (2009)
Youssef Hamadi, Saïd Jabbour, Lakhdar Saïs
*Portfolio-based parallel solving.*

---

## Online Courses & Lectures

### Video Lectures

**[Automated Reasoning - Stanford CS257](https://web.stanford.edu/class/cs257/)**
*Graduate course covering SAT, SMT, and automated reasoning.*

**[SAT/SMT by Example - Dennis Yurichev](https://yurichev.com/writings/SAT_SMT_by_example.pdf)**
*Practical examples of using SAT/SMT solvers (400+ page book/tutorial).*

**[Introduction to Computational Logic - TU Dresden](https://iccl.inf.tu-dresden.de/web/Einf%C3%BChrung_in_die_Computational_Logic_(SS2020)/en)**
*Includes excellent SAT solving lectures.*

### Online Tutorials

**[SAT Live!](http://www.satlive.org/)**
*Community website with solver downloads, benchmarks, and news.*

**[Practical SAT Solving (Lecture Notes)](https://baldur.iti.kit.edu/sat/)**
*Armin Biere and Marijn Heule's lecture materials.*

**[Modern SAT Solvers: Fast, Neat and Underused (Part 1 of N)](https://codingnest.com/modern-sat-solvers-fast-neat-underused-part-1-of-n/)**
*Practical blog series on using SAT solvers.*

---

## Websites & Tools

### Solver Implementations

**[MiniSat](http://minisat.se/)**
*The minimalist CDCL solver - excellent for learning.*

**[CryptoMiniSat](https://github.com/msoos/cryptominisat)**
*Modern solver with XOR reasoning for cryptography.*

**[Glucose](https://www.labri.fr/perso/lsimon/glucose/)**
*Fast CDCL solver based on MiniSat.*

**[Lingeling, Plingeling, Treengeling](http://fmv.jku.at/lingeling/)**
*Family of competitive solvers by Armin Biere.*

**[CaDiCaL](https://github.com/arminbiere/cadical)**
*Simplified CDCL solver for research and competition.*

**[Z3](https://github.com/Z3Prover/z3)**
*Microsoft's SMT solver (includes SAT).*

### Benchmarks & Instances

**[SATLIB - Benchmark Problems](https://www.cs.ubc.ca/~hoos/SATLIB/benchm.html)**
*Classic SAT benchmark library.*

**[SAT Competition Benchmarks](http://www.satcompetition.org/)**
*Instances from annual competitions.*

**[DIMACS CNF Format](http://www.satcompetition.org/2009/format-benchmarks2009.html)**
*Standard format for SAT instances.*

### Visualization Tools

**[SAT Heritage](http://www.satlive.org/)**
*Interactive visualization of SAT solver timeline.*

**[Boolean Satisfiability Visualizer](https://github.com/tuzz/satisfiability)**
*Interactive SAT formula visualizations.*

---

## Competitions & Benchmarks

### Annual Competitions

**[SAT Competition](http://www.satcompetition.org/)**
*Annual competition since 2002. Source of state-of-the-art solvers.*

**[SAT Race](http://sat-race.gi.ulg.ac.be/)**
*Parallel/concurrent SAT solver competition.*

**[MaxSAT Evaluation](https://maxsat-evaluations.github.io/)**
*Competition for optimization variants of SAT.*

### Application Tracks

**[Hardware Model Checking Competition](http://fmv.jku.at/hwmcc/)**
*SAT-based verification benchmarks.*

**[Configurable SAT Solver Challenge](http://www.cs.ubc.ca/labs/beta/Projects/CSSC/)**
*Automated algorithm configuration for SAT.*

---

## Related Topics

### SMT (Satisfiability Modulo Theories)

**[Satisfiability Modulo Theories: Introduction and Applications](https://dl.acm.org/doi/10.1145/1995376.1995394)** (2011)
Leonardo de Moura and Nikolaj Bjørner
*Introduction to SMT solving.*

**[SMT-LIB: The Satisfiability Modulo Theories Library](http://smtlib.cs.uiowa.edu/)**
*Standard formats and benchmarks for SMT.*

### #SAT (Model Counting)

**[A Sharp-SAT Solver](https://dl.acm.org/doi/10.1007/11527695_8)** (2005)
Marc Thurley
*Counting models of SAT formulas.*

**[ApproxMC: Approximate Model Counter](https://link.springer.com/chapter/10.1007/978-3-642-54862-8_24)** (2014)
*Approximate counting for large formulas.*

### QBF (Quantified Boolean Formulas)

**[Solving and Verifying the Boolean Pythagorean Triples Problem via Cube-and-Conquer](https://link.springer.com/chapter/10.1007/978-3-319-40970-2_15)** (2016)
Marijn J.H. Heule, Oliver Kullmann, Victor W. Marek
*Using SAT to solve open math problems.*

### MaxSAT (Optimization)

**[MaxSAT Evaluation](https://maxsat-evaluations.github.io/)**
*Finding maximum satisfiable subsets.*

**[Algorithms for Maximum Satisfiability](https://www.cs.helsinki.fi/group/coreo/maxsat/)**
*Survey of MaxSAT techniques.*

### Applications

**[Applications of SAT Solvers to Cryptanalysis of Hash Functions](https://link.springer.com/chapter/10.1007/11716723_18)** (2006)
*Using SAT for cryptography.*

**[Pairwise Testing in Software Product Lines](https://link.springer.com/chapter/10.1007/978-3-642-02652-2_19)** (2009)
*SAT for software testing.*

**[Planning as Satisfiability](https://www.aaai.org/Papers/ECAI/1992/ECAI92-155.pdf)** (1992)
*Encoding planning problems as SAT.*

---

## Research Groups

### Leading SAT Research Labs

**[Formal Methods Group - JKU Linz](http://fmv.jku.at/)** (Armin Biere)
*CaDiCaL, Lingeling, and more.*

**[Constraints & Verification Group - University of Lisbon](http://sat.inesc-id.pt/)** (João Marques-Silva)
*SAT applications and theory.*

**[Automated Reasoning Group - UCSD](https://scg.ucsd.edu/)** (Vijay Ganesh)
*Understanding SAT heuristics.*

**[Reliable and Intelligent Systems Lab - Helsinki](https://www.cs.helsinki.fi/group/coreo/)** (Matti Järvisalo)
*MaxSAT and preprocessing.*

**[Formal Verification Group - Princeton](https://www.princeton.edu/~chaff/)** (Sharad Malik)
*Chaff and hardware verification.*

---

## Historical Timeline

- **1960**: Davis-Putnam algorithm
- **1962**: DPLL algorithm (Davis, Logemann, Loveland)
- **1971**: Cook-Levin theorem (SAT is NP-complete)
- **1979**: 2-SAT solved in linear time (Aspvall et al.)
- **1992**: GSAT local search (Selman et al.)
- **1994**: WalkSAT noise strategies
- **1996**: GRASP - first CDCL solver
- **2001**: Chaff - VSIDS + watched literals
- **2003**: MiniSat - minimalist reference implementation
- **2005**: SatELite preprocessing
- **2009**: Glucose LBD restart strategy
- **2013**: Lingeling - modern competition winner
- **2016**: Boolean Pythagorean Triples solved with SAT
- **2020+**: Machine learning for SAT solving

---

## How to Get Started

### For Beginners

1. Start with **Sipser's Introduction to Theory of Computation** for NP-completeness
2. Read **SAT Solvers: A Tutorial** by Marques-Silva
3. Implement basic DPLL following **Knuth's Volume 4B**
4. Study **MiniSat source code** for CDCL
5. Try **SAT/SMT by Example** for practical applications

### For Researchers

1. Read **Handbook of Satisfiability** (comprehensive reference)
2. Study competition-winning solvers (CaDiCaL, Kissat)
3. Follow **SAT Competition** results and techniques
4. Join **SAT Live** community
5. Read recent **CAV, SAT, CP conference** papers

### For Practitioners

1. Learn **DIMACS CNF format**
2. Try **Z3** or **MiniSat** on simple problems
3. Read **SAT/SMT by Example** tutorials
4. Explore applications in your domain
5. Use **portfolio solvers** for hard instances

---

## Conclusion

SAT solving is a rich field combining theory, algorithms, and engineering. This reading list provides entry points for all levels, from beginners to researchers. The field continues to evolve with new techniques, applications, and theoretical insights.

**Happy reading! 📚**

For more resources, visit:
- [SAT Live Community](http://www.satlive.org/)
- [BSAT Documentation](README.md)
- [Theory & References](theory.md)
