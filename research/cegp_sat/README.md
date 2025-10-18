# CEGP-SAT: Clause Evolution with Genetic Programming

**EDUCATIONAL/EXPERIMENTAL: Genetic programming for clause evolution**

Evolves learned clauses using crossover and mutation to discover high-quality clauses for propagation.

## Citation and Attribution

### 🧪 **EXPERIMENTAL** - Educational Demonstration

This implementation demonstrates genetic programming applied to SAT solving.

**Concept**:
- **Genetic Programming**: Evolutionary algorithms for program/structure evolution
- **Clause Learning**: CDCL learns clauses from conflicts
- **Combination**: Evolve learned clauses to improve propagation

**This Implementation**:
- Educational demonstration of genetic operators for SAT
- Crossover: Combine literals from two parent clauses
- Mutation: Modify literals (flip, replace, add, remove)
- Fitness: Based on propagation effectiveness and conflict participation

**NOT a Standard Technique**: Genetic programming for clause evolution is experimental/educational. Traditional CDCL is more reliable.

---

## Overview

Standard CDCL learns clauses from conflicts. **CEGP-SAT** adds genetic evolution:

1. **Learn clauses**: Standard conflict analysis
2. **Evaluate fitness**: Track propagation counts and conflict participation
3. **Evolve**: Apply crossover and mutation to create variants
4. **Select**: Keep high-fitness clauses in population

### Key Insight

> Learned clauses vary in quality. Genetic operators can potentially discover clause variants that propagate more effectively.

**Example**: Clause Evolution
```
Population of learned clauses:
  C1: (a | b | c)        fitness = 0.7 (propagated 5 times)
  C2: (~a | d)           fitness = 0.9 (propagated 8 times)
  C3: (b | ~d | e)       fitness = 0.4 (propagated 2 times)

Crossover (C1, C2):
  Offspring1: (a | b)              ← first half of C1
  Offspring2: (~a | d | c)         ← first half of C2 + second of C1

Mutation (C2):
  Mutated: (~a | ~d)               ← flipped polarity of d

Add evolved clauses to population
Evaluate fitness
Keep top performers
```

---

## Algorithm

### Genetic Operators

```python
def crossover(parent1, parent2):
    """
    One-point crossover: split clauses and combine.

    Example:
      Parent1: (a | b | c | d)
      Parent2: (e | f | g)
      Point1 = 2, Point2 = 1

      Offspring1: (a | b) + (f | g) = (a | b | f | g)
      Offspring2: (e) + (c | d) = (e | c | d)
    """
    point1 = random.randint(0, len(parent1.literals))
    point2 = random.randint(0, len(parent2.literals))

    offspring1 = parent1.literals[:point1] + parent2.literals[point2:]
    offspring2 = parent2.literals[:point2] + parent1.literals[point1:]

    return make_clause(offspring1), make_clause(offspring2)


def mutate(clause, available_vars):
    """
    Mutate clause with various operations.

    Operations:
    - Flip polarity: (a | b) → (~a | b)
    - Replace variable: (a | b) → (c | b)
    - Add literal: (a | b) → (a | b | c)
    - Remove literal: (a | b | c) → (a | b)
    """
    mutated = list(clause.literals)

    for i, lit in enumerate(mutated):
        if random() < mutation_rate:
            if choice(['flip', 'replace']) == 'flip':
                mutated[i] = Literal(lit.variable, not lit.negated)
            else:
                new_var = choice(available_vars)
                mutated[i] = Literal(new_var, choice([True, False]))

    # Occasionally add/remove
    if random() < mutation_rate:
        if len(mutated) > 1:
            mutated.pop(random_index)
        else:
            mutated.append(random_literal(available_vars))

    return make_clause(mutated)
```

### Fitness Evaluation

```python
def compute_fitness(clause):
    """
    Fitness based on propagation effectiveness.

    Metrics:
    - Propagation count: How often clause propagates
    - Conflict participation: Used in conflict analysis
    - Size: Shorter clauses preferred
    """
    size_score = max(0, 1 - len(clause) / 10)
    prop_score = min(1, propagation_count / 10)
    conflict_score = min(1, conflict_participation / 5)

    fitness = (
        size_score * 0.3 +      # 30%: size
        prop_score * 0.5 +      # 50%: propagation
        conflict_score * 0.2    # 20%: conflicts
    )

    return fitness
```

### Evolution Loop

```python
def evolve_clauses(population):
    """
    Evolve clause population using genetic programming.

    1. Evaluate fitness of all clauses
    2. Select top performers
    3. Apply crossover to create offspring
    4. Apply mutation to variants
    5. Add evolved clauses to database
    """
    # Evaluate fitness
    population = [(clause, compute_fitness(clause)) for clause in population]
    population.sort(key=lambda x: x[1], reverse=True)

    # Crossover: create offspring
    new_clauses = []
    for i in range(num_crossovers):
        parent1 = tournament_selection(population)
        parent2 = tournament_selection(population)
        offspring1, offspring2 = crossover(parent1, parent2)
        new_clauses.extend([offspring1, offspring2])

    # Mutation: mutate top clauses
    for clause, _ in population[:num_mutations]:
        mutated = mutate(clause, available_vars)
        new_clauses.append(mutated)

    # Add to database
    for clause in new_clauses:
        if not is_tautology(clause):
            add_learned_clause(clause)
```

---

## Usage

### Basic Usage

```python
from bsat import CNFExpression
from research.cegp_sat import CEGPSATSolver

# Parse formula
cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")

# Create solver with clause evolution
solver = CEGPSATSolver(
    cnf,
    use_evolution=True,
    evolution_frequency=100,  # Evolve every 100 conflicts
    population_size=20,       # Max 20 evolved clauses
    crossover_rate=0.7,       # 70% crossover probability
    mutation_rate=0.1         # 10% mutation per literal
)

# Solve
result = solver.solve()

if result:
    print(f"SAT: {result}")

    # Get evolution statistics
    evo_stats = solver.get_evolution_statistics()
    print(f"Evolutions: {evo_stats['evolutions']}")
    print(f"Evolved clauses: {evo_stats['evolved_clauses']}")
```

---

## When This Approach Works

**✅ Potentially useful for**:
- Exploring clause search space
- Problems where clause quality matters significantly
- Educational/research purposes

**❌ Generally less effective than standard CDCL**:
- Random evolution can introduce low-quality clauses
- Overhead of fitness tracking and genetic operations
- No theoretical guarantee of improvement
- Traditional CDCL already very effective

**Experimental**: This is a research/educational implementation. Effectiveness varies significantly by problem structure.

---

## Complexity

**Time**:
- Crossover: O(clause length) per operation
- Mutation: O(clause length) per operation
- Fitness evaluation: O(1) per clause (tracked incrementally)
- Evolution overhead: ~5-10% per evolution round

**Space**:
- Fitness tracking: O(learned clauses)
- Population: Limited to population_size

---

## Implementation Details

### Components

1. **GeneticOperators** (`genetic_operators.py`):
   - Crossover: One-point crossover of clause literals
   - Mutation: Flip, replace, add, remove literals
   - Selection: Tournament selection based on fitness

2. **FitnessEvaluator** (`fitness_evaluator.py`):
   - ClauseFitness: Tracks propagation and conflict participation
   - compute_fitness(): Combines size, propagation, conflict metrics
   - get_top_clauses(): Returns best performers

3. **CEGPSATSolver** (`cegp_solver.py`):
   - Extends CDCLSolver with evolution
   - Periodic evolution (every N conflicts)
   - Adds evolved clauses to learned clause database
   - Extended statistics

### Metrics Tracked

- `evolutions`: Number of evolution rounds
- `evolved_clauses`: Total evolved clauses added
- `crossovers`: Crossover operations performed
- `mutations`: Mutation operations performed
- `propagation_count`: Per-clause propagation tracking
- `conflict_participation`: Per-clause conflict tracking

---

## Limitations

1. **Randomness**: Evolution is stochastic, results vary
2. **Quality**: No guarantee evolved clauses help
3. **Overhead**: Genetic operations add computational cost
4. **Theory**: Lacks formal SAT solving guarantees

**Recommendation**: Use standard CDCL for production. Use CEGP-SAT for education/research/experimentation.

---

## Conclusion

CEGP-SAT is an **educational/experimental** implementation demonstrating genetic programming for clause evolution:

**🧪 Experimental**: Not a standard SAT technique
**✅ Educational**: Shows GP applied to SAT
**✅ Well-Documented**: Explains genetic operators
**❌ Not Recommended for Production**: Use standard CDCL

### Key Takeaway

Genetic programming can evolve learned clauses through crossover and mutation, potentially discovering high-quality variants. However, this is experimental - traditional CDCL is more reliable.

### When to Use

- **Education/Research**: Understanding evolutionary algorithms
- **Experimentation**: Exploring clause search space
- **NOT Production**: Use modern SAT solvers (Glucose, CryptoMiniSat, Kissat)

**Bottom Line**: This is an **experimental teaching tool** demonstrating genetic programming for SAT. Not recommended for production use.

---

## Examples

See `example.py` for working demonstrations:
- Simple SAT with clause evolution
- Comparing with/without evolution
- Aggressive evolution parameters
- Fitness analysis

Run examples:
```bash
python3 research/cegp_sat/example.py
```
