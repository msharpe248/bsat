# MARKET-SAT: Economic Auction-Based SAT Solver

## Economic Inspiration

### Market Mechanisms
Markets efficiently allocate scarce resources among competing agents through price signals:

- **Price Discovery**: Auction mechanisms reveal true value of goods
- **Equilibrium**: Supply and demand balance at market-clearing price
- **Incentive Compatibility**: Truthful bidding is optimal strategy (VCG auctions)
- **Distributed Computation**: No central planner needed
- **Nash Equilibrium**: Stable outcome where no agent wants to deviate

### Classical Economics Foundations

1. **Walrasian Equilibrium**: Prices adjust until all markets clear
2. **Vickrey-Clarke-Groves (VCG)**: Truthful auction mechanism
3. **Tatonnement Process**: Iterative price adjustment toward equilibrium
4. **Game Theory**: Strategic interaction between rational agents
5. **Mechanism Design**: Creating rules that produce desired outcomes

## SAT Problem Mapping

### Core Metaphor
**Finding a satisfying assignment = Reaching market equilibrium where all clauses (buyers) successfully acquire satisfying literals (goods)**

### Detailed Mapping

| SAT Element | Economic Analog | Interpretation |
|-------------|-----------------|----------------|
| **Variable** | Commodity/Good | Tradeable item with two variants (T, F) |
| **Assignment (T/F)** | Allocation | Which variant is sold/assigned |
| **Clause** | Buyer/Bidder | Agent wanting to acquire satisfying literal |
| **Literal** | Product variant | Specific form of commodity (x or ~x) |
| **Clause Size** | Buying power | More literals = more options = stronger buyer |
| **Satisfaction** | Successful purchase | Clause acquired at least one desired literal |
| **Price** | Variable priority | High price = high demand variable |
| **Budget** | Clause importance | Resources clause can spend |
| **Market Clearing** | SAT solution | All buyers satisfied, all goods allocated |
| **UNSAT** | Market failure | No equilibrium exists (excess demand) |

### Economic Intuition

```
Example: (x1 ∨ x2) ∧ (~x1 ∨ x3)

Clause 1 wants to buy: x1=T OR x2=T
Clause 2 wants to buy: x1=F OR x3=T

Market dynamics:
- If x1 price too high, buyers choose alternatives (x2, x3)
- If x1 popular, price rises, forcing clauses to compete
- Equilibrium: Prices such that all clauses can afford satisfaction
```

## Algorithm Design

### Phase 1: Market Initialization

1. **Create Commodity Market**: One market per variable (selling T or F)
2. **Instantiate Buyer Agents**: One bidding agent per clause
3. **Allocate Budgets**: Each clause gets budget based on clause difficulty
   - Unit clauses (size 1): High budget (must win)
   - Large clauses (size > 3): Lower budget (many options)
4. **Initialize Prices**: All variables start at base price (1.0)

### Phase 2: Auction Mechanism

**Ascending Clock Auction** (inspired by spectrum auctions):

```
For each variable x:
    Price starts low
    Clauses indicate demand (I want x=T or x=F)
    Price rises if excess demand
    Clauses drop out when price exceeds value
    Auction ends when demand = supply (one value wins)
```

**Simultaneous Ascending Auction**: All variables auctioned in parallel

### Phase 3: Bidding Strategy

**Clause Agent Decision**:
```python
def clause_bid_decision(clause, prices):
    """Decide which literal to bid on."""

    # Evaluate all literals in clause
    best_literal = None
    best_value = -infinity

    for literal in clause.literals:
        # Value = benefit - cost
        benefit = satisfaction_from_acquiring(literal)
        cost = current_price(literal)

        net_value = benefit - cost

        # Bid on highest-value literal
        if net_value > best_value:
            best_value = net_value
            best_literal = literal

    # If no literal has positive value, budget depleted
    if best_value <= 0:
        return None  # Drop out of auction

    return best_literal
```

### Phase 4: Price Update (Tatonnement)

**Excess Demand Function**:
```
ED(x=T) = (# clauses wanting x=T) - 1
ED(x=F) = (# clauses wanting x=F) - 1
```

(Supply is always 1: each variable has one "true" assignment)

**Price Adjustment**:
```python
def update_prices(market, learning_rate=0.1):
    """Adjust prices based on excess demand."""

    for variable in market.variables:
        # Count demand for T and F
        demand_true = count_bids_for(variable, value=True)
        demand_false = count_bids_for(variable, value=False)

        # Excess demand
        excess_true = demand_true - 1
        excess_false = demand_false - 1

        # Raise price if excess demand
        market.price[variable][True] += learning_rate * max(0, excess_true)
        market.price[variable][False] += learning_rate * max(0, excess_false)

        # Prices can't go negative
        market.price[variable][True] = max(0.01, market.price[variable][True])
        market.price[variable][False] = max(0.01, market.price[variable][False])
```

### Phase 5: Market Clearing (Equilibrium)

**Equilibrium Condition**:
```
For all variables x:
    Exactly one of {x=T, x=F} has demand = 1
    Other value has demand = 0

AND

All clauses have acquired at least one literal (all satisfied)
```

**Assignment Extraction**:
```python
def extract_assignment_from_market(market):
    """Extract variable assignment from market allocation."""
    assignment = {}

    for variable in market.variables:
        # Assign to value with positive demand
        if market.demand[variable][True] > 0:
            assignment[variable] = True
        elif market.demand[variable][False] > 0:
            assignment[variable] = False
        else:
            # No demand for either (shouldn't happen at equilibrium)
            assignment[variable] = True  # Arbitrary

    return assignment
```

## Implementation Strategy

### Data Structures

```python
class Market:
    """Economic market for SAT solving."""

    variables: List[str]                    # All variables
    price: Dict[str, Dict[bool, float]]     # price[var][True/False]
    demand: Dict[str, Dict[bool, int]]      # demand[var][True/False]
    supply: int = 1                         # Each variable assigned once

class ClauseBidder:
    """Buyer agent representing a clause."""

    clause: Clause                          # SAT clause
    budget: float                           # Spending limit
    spent: float                            # Amount spent so far
    allocated_literal: Optional[Literal]    # Winning literal (if any)

    def valuation(self, literal: Literal) -> float:
        """How much clause values this literal."""
        # Unit clause: High valuation (critical)
        if len(self.clause.literals) == 1:
            return 1000.0

        # Otherwise: Inversely proportional to clause size
        return 100.0 / len(self.clause.literals)

    def can_afford(self, literal: Literal, price: float) -> bool:
        """Can clause afford to buy this literal?"""
        return price <= (self.budget - self.spent)

    def place_bid(self, literal: Literal, price: float):
        """Place bid on literal at given price."""
        if not self.can_afford(literal, price):
            return False

        self.allocated_literal = literal
        self.spent = price
        return True
```

### Core Algorithm

```python
def solve_market_sat(cnf, max_rounds=1000):
    """Solve SAT using market auction mechanism."""

    # Initialize market
    market = Market(cnf.get_variables())

    # Create buyer agents (one per clause)
    bidders = [ClauseBidder(clause) for clause in cnf.clauses]

    # Assign budgets
    for bidder in bidders:
        bidder.budget = compute_budget(bidder.clause)

    # Initialize prices
    for var in market.variables:
        market.price[var][True] = 1.0
        market.price[var][False] = 1.0

    # Tatonnement process
    for round in range(max_rounds):
        # Phase 1: Bidders select preferred literals
        market.reset_demand()

        for bidder in bidders:
            chosen_literal = bidder.choose_best_literal(market.price)

            if chosen_literal:
                # Register demand
                var = chosen_literal.variable
                value = not chosen_literal.negated
                market.demand[var][value] += 1

        # Phase 2: Check if equilibrium reached
        if market.is_equilibrium():
            # All clauses satisfied?
            if all(bidder.allocated_literal for bidder in bidders):
                assignment = market.extract_assignment()
                return assignment  # SAT!
            else:
                # Some clauses unsatisfied (budget exhausted)
                return None  # UNSAT (no affordable allocation)

        # Phase 3: Update prices based on excess demand
        market.update_prices()

        # Phase 4: Budget adjustment (help struggling clauses)
        for bidder in bidders:
            if bidder.allocated_literal is None:
                # Clause couldn't afford anything - give subsidy
                bidder.budget *= 1.1  # 10% budget increase

    # Didn't converge
    return None
```

### Key Functions

**Budget Computation**:
```python
def compute_budget(clause):
    """Assign budget based on clause difficulty."""

    # Unit clauses must succeed - unlimited budget
    if len(clause.literals) == 1:
        return float('inf')

    # Binary clauses - high priority
    if len(clause.literals) == 2:
        return 100.0

    # Longer clauses - proportionally less budget
    return 100.0 / len(clause.literals)
```

**Equilibrium Check**:
```python
def is_equilibrium(market):
    """Check if market has reached equilibrium."""

    for var in market.variables:
        demand_true = market.demand[var][True]
        demand_false = market.demand[var][False]

        # Exactly one value should have demand = 1
        if not ((demand_true == 1 and demand_false == 0) or
                (demand_true == 0 and demand_false == 1)):
            return False

    return True
```

**Literal Selection**:
```python
def choose_best_literal(bidder, prices):
    """Choose highest-value affordable literal."""

    best_literal = None
    best_surplus = -infinity

    for literal in bidder.clause.literals:
        var = literal.variable
        value = not literal.negated
        price = prices[var][value]

        # Consumer surplus = valuation - price
        valuation = bidder.valuation(literal)
        surplus = valuation - price

        # Can afford and best surplus so far?
        if bidder.can_afford(literal, price) and surplus > best_surplus:
            best_surplus = surplus
            best_literal = literal

    return best_literal
```

## Advantages

1. **Game-Theoretic Foundations**: Rigorously defined equilibrium concept
2. **Natural Priorities**: Unit clauses automatically prioritized (high valuation)
3. **Distributed**: Each clause agent acts independently
4. **Incentive Compatible**: Truthful bidding is optimal (VCG property)
5. **Interpretable**: Prices reveal variable "importance"
6. **Adaptive**: Struggling clauses receive budget boosts

## Challenges

1. **Convergence**: Tatonnement not guaranteed to converge for all problems
2. **UNSAT Detection**: Hard to distinguish "no equilibrium" from "slow convergence"
3. **Parameter Tuning**: Budget allocation crucial for performance
4. **Computational Overhead**: Auction simulation overhead per iteration

## Extensions

### 1. **VCG Auction** (Vickrey-Clarke-Groves)
Instead of ascending auction, use VCG for truthful mechanism:
```python
# Each clause reports true valuation
# Winner pays second-highest bid (Vickrey)
# Incentivizes truth-telling
```

### 2. **Combinatorial Auction**
Clauses bid on bundles of literals (for complex preferences):
```python
# Clause bids on {x1=T, x2=T} as package
# More expressive but harder to solve
```

### 3. **Budget Reallocation**
Dynamically transfer budget from satisfied clauses to struggling ones:
```python
# Robin Hood mechanism
if clause.is_satisfied():
    clause.budget *= 0.9
    unsatisfied_clauses.budget += 0.1
```

### 4. **Price Discrimination**
Different prices for different clauses (personalized pricing):
```python
# Unit clauses pay lower prices (subsidy)
# Long clauses pay market price
```

## Theoretical Connections

### Nash Equilibrium as SAT
**Theorem**: A SAT solution corresponds to Nash equilibrium in the bidding game

**Proof Sketch**:
- Each clause's strategy: choose literal to bid on
- Nash equilibrium: No clause wants to change bid (all satisfied)
- Correspondence: Nash equilibrium ↔ satisfying assignment

### Mechanism Design
MARKET-SAT is a **mechanism**: rules defining how agents interact

**Desirable properties**:
1. **Efficiency**: Maximize social welfare (clause satisfaction)
2. **Incentive Compatibility**: Truth-telling is optimal
3. **Budget Balance**: No external subsidy needed
4. **Individual Rationality**: Participation voluntary

Our auction approximately achieves these!

### Connection to Linear Programming
Market equilibrium ↔ LP solution:
```
Maximize: Σ(clause satisfaction)
Subject to:
    - Budget constraints
    - Supply = demand
    - Variables ∈ {0, 1}
```

## References

- Arrow, K.J. & Debreu, G. (1954). "Existence of Equilibrium for Competitive Economy"
- Vickrey, W. (1961). "Counterspeculation, Auctions, and Competitive Sealed Tenders"
- Clarke, E.H. (1971). "Multipart Pricing of Public Goods"
- Groves, T. (1973). "Incentives in Teams"
- Milgrom, P. (2004). "Putting Auction Theory to Work"

## Educational Value

This approach demonstrates:
- **Mechanism Design**: How to design systems with good incentives
- **Game Theory**: Strategic reasoning and equilibrium
- **Economics**: Price discovery and resource allocation
- **Distributed Algorithms**: Decentralized problem-solving

**Novel Contribution**: First application of auction theory and market mechanisms to SAT solving!

**Why It Might Work**: Markets are incredibly good at solving hard optimization problems (e.g., stock markets, spectrum auctions). SAT is fundamentally about resource allocation—why not use market mechanisms?
