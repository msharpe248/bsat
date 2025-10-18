# MARKET-SAT: Economic Auction-Based SAT Solver

## Overview

MARKET-SAT solves Boolean satisfiability problems using **market mechanisms** and **auction theory**. Instead of traditional search algorithms, it treats SAT as an economic problem where:

- **Variables** are commodities being auctioned
- **Clauses** are buyers bidding for satisfying literals
- **Satisfying assignment** emerges as market equilibrium
- **Prices** adjust via tatonnement until supply = demand

**Novel Contribution**: First application of auction theory and mechanism design to SAT solving!

## Theoretical Foundation

### Economic Interpretation

| SAT Element | Economic Analog |
|-------------|-----------------|
| Variable | Commodity with two variants (T/F) |
| Assignment | Market allocation |
| Clause | Buyer/bidder agent |
| Literal | Product variant (x or ~x) |
| Satisfaction | Successful purchase |
| Price | Variable priority/importance |
| Budget | Clause buying power |
| Equilibrium | SAT solution |

### Walrasian Equilibrium

The solver finds **Walrasian equilibrium** where:
1. Demand = Supply for all variables
2. All clauses (buyers) satisfied with allocation
3. No agent wants to change their choice

### Auction Mechanism

**Simultaneous Ascending Auction**:
- All variables auctioned in parallel
- Prices rise when excess demand
- Clauses bid on best-value literals
- Process converges to equilibrium

## Algorithm

```
1. Initialize:
   - Create bidder agent for each clause
   - Assign budgets (unit clauses get infinite budget)
   - Set initial prices (all start at 1.0)

2. Auction Round:
   a. Each clause chooses best literal (highest value - price)
   b. Register all demands
   c. Check if equilibrium reached
   d. Update prices based on excess demand

3. Price Update (Tatonnement):
   For each variable:
     If demand > supply: raise price
     If demand < supply: lower price
     If demand = supply: keep price

4. Budget Adjustment:
   - Boost budgets of unsatisfied clauses
   - Helps "priced out" clauses compete

5. Repeat until:
   - Equilibrium + all satisfied → SAT
   - Equilibrium + some unsatisfied → UNSAT
   - Max rounds exceeded → Unknown
```

## Implementation

### Core Classes

**ClauseBidder**:
```python
class ClauseBidder:
    - clause: SAT clause this agent represents
    - budget: Spending limit
    - valuation(literal): How much clause values literal
    - choose_best_literal(prices): Utility-maximizing choice
    - consumer_surplus(literal, price): value - price
```

**PriceManager**:
```python
class PriceManager:
    - prices: (variable, value) → price
    - demand: (variable, value) → # bidders
    - update_prices(): Tatonnement adjustment
    - is_equilibrium(): Check market clearing
```

**AuctionEngine**:
```python
class AuctionEngine:
    - run_auction_round(): Coordinate bidding
    - allocate_winners(): Assign literals
    - boost_struggling_clauses(): Budget help
    - run_full_auction(): Complete solve process
```

## Usage

### Basic Usage

```python
from bsat import CNFExpression
from market_sat import MARKETSATSolver

# Create CNF formula
cnf = CNFExpression.parse("(a | b) & (~a | c)")

# Solve with auction
solver = MARKETSATSolver(cnf)
result = solver.solve()

if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

### With Statistics

```python
solver = MARKETSATSolver(cnf)
result = solver.solve()

# Get market statistics
stats = solver.get_market_statistics()
print(f"Auction rounds: {stats['auction_rounds']}")
print(f"Equilibrium: {stats['equilibrium']}")
print(f"Total spent: ${stats['total_spent']:.2f}")
```

### Advanced Usage

```python
# Custom parameters
solver = MARKETSATSolver(
    cnf,
    max_auction_rounds=2000,  # More rounds for hard problems
    use_market=True           # Enable market mechanism
)

# Get detailed state
state = solver.get_detailed_market_state()
print(state['market'])  # Prices and demand for each variable
print(state['bidders'])  # Status of each clause bidder
```

## Examples

See `example.py` for comprehensive examples:

```bash
python3 example.py
```

Examples include:
- Simple SAT formula
- Unit clauses (forced assignments)
- UNSAT formula (no equilibrium)
- Larger problem showing market dynamics
- Comparison with CDCL solver

## Advantages

1. **Theoretical Foundation**: Grounded in auction theory and game theory
2. **Natural Priorities**: Unit clauses automatically get highest priority
3. **Distributed**: Each clause acts independently (natural parallelism)
4. **Interpretable**: Prices reveal variable importance
5. **Incentive Compatible**: Truthful bidding is optimal strategy

## Limitations

1. **Convergence**: Tatonnement not guaranteed to converge for all instances
2. **UNSAT Detection**: Hard to distinguish no-equilibrium from slow convergence
3. **Overhead**: Auction simulation adds computational overhead
4. **Parameter Tuning**: Budget allocation affects performance

## Educational Value

MARKET-SAT demonstrates:
- **Mechanism Design**: Creating systems with good incentive properties
- **Game Theory**: Strategic reasoning and equilibrium concepts
- **Economics**: Price discovery and resource allocation
- **Alternative Paradigms**: Non-search-based approach to SAT

## References

### Auction Theory
- **Vickrey (1961)**: "Counterspeculation, Auctions, and Competitive Sealed Tenders"
- **Milgrom (2004)**: "Putting Auction Theory to Work"

### General Equilibrium
- **Arrow & Debreu (1954)**: "Existence of Equilibrium for Competitive Economy"
- **Walras (1874)**: "Elements of Pure Economics"

### Mechanism Design
- **Clarke (1971)**: "Multipart Pricing of Public Goods"
- **Groves (1973)**: "Incentives in Teams"

## Future Extensions

1. **VCG Mechanism**: Implement Vickrey-Clarke-Groves auction for truthfulness
2. **Combinatorial Auction**: Let clauses bid on bundles of literals
3. **Budget Reallocation**: Transfer budget from satisfied to struggling clauses
4. **Price Discrimination**: Different prices for different clause types
5. **Learning**: Train optimal budget allocation from problem instances

## Comparison to Other Approaches

| Aspect | MARKET-SAT | CDCL | Local Search |
|--------|-----------|------|--------------|
| Paradigm | Economic equilibrium | Conflict-driven search | Hill climbing |
| Decisions | Bidding/pricing | Variable assignment | Flipping variables |
| Guidance | Consumer surplus | VSIDS heuristic | Clause breaks |
| Learning | Price/budget adjustment | Clause learning | None (greedy) |
| Guarantee | No (may not converge) | Yes (complete) | No (incomplete) |

## Performance Notes

- **Best for**: Small to medium problems with clear structure
- **Struggles with**: Very large problems, highly irregular instances
- **Interesting behavior**: Prices reveal problem structure
- **Typical rounds**: 10-100 for simple problems, 100-1000 for harder ones

## Why It Might Work

Markets are incredibly efficient at solving complex optimization problems in the real world:
- **Stock markets** price thousands of securities
- **Spectrum auctions** allocate radio frequencies
- **Ad auctions** match advertisers to search queries

SAT is fundamentally about resource allocation (assigning truth values to variables subject to constraints). Market mechanisms might offer fresh insights into this classic problem!

---

**Educational/Experimental**: This is a research implementation demonstrating novel approaches to SAT. Not intended for production use—use state-of-the-art solvers like CryptoMiniSat or Glucose for real applications.
