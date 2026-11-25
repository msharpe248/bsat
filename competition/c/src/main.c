/*********************************************************************
 * BSAT Competition Solver - Main Entry Point
 *********************************************************************/

#include "../include/solver.h"
#include "../include/dimacs.h"
#include "../include/pb_enc.h"
#include "../include/maxsat.h"
#include "../include/queries.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <signal.h>

/*********************************************************************
 * Signal Handling for Progress Monitoring
 *********************************************************************/

// Global flag to request statistics dump (set by signal handler)
static volatile sig_atomic_t print_stats_requested = 0;

// Signal handler for SIGUSR1 - request statistics dump
static void sigusr1_handler(int signum) {
    (void)signum;  // Unused parameter
    print_stats_requested = 1;
}

// Install signal handler
static void install_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
}

// Print progress statistics (safe to call from main loop)
static void print_progress_stats(const Solver* s) {
    double elapsed = solver_cpu_time() - s->stats.start_time;
    fprintf(stderr, "\n");
    fprintf(stderr, "c ========== Progress Update ==========\n");
    fprintf(stderr, "c Elapsed time     : %.3f s\n", elapsed);
    fprintf(stderr, "c Decisions        : %llu\n", (unsigned long long)s->stats.decisions);
    fprintf(stderr, "c Propagations     : %llu\n", (unsigned long long)s->stats.propagations);
    fprintf(stderr, "c Conflicts        : %llu\n", (unsigned long long)s->stats.conflicts);
    fprintf(stderr, "c Restarts         : %llu\n", (unsigned long long)s->stats.restarts);
    fprintf(stderr, "c Learned clauses  : %llu\n", (unsigned long long)s->stats.learned_clauses);
    fprintf(stderr, "c Decision level   : %u\n", s->decision_level);
    fprintf(stderr, "c Trail size       : %u\n", s->trail_size);
    if (elapsed > 0) {
        fprintf(stderr, "c Conflicts/sec    : %.0f\n", s->stats.conflicts / elapsed);
        fprintf(stderr, "c Decisions/sec    : %.0f\n", s->stats.decisions / elapsed);
    }
    fprintf(stderr, "c ======================================\n");
    fprintf(stderr, "\n");
    fflush(stderr);
}

static bool cli_print_model_cb(void *ctx, const lbool *model, uint32_t num_vars, uint64_t idx) {
    bool quiet = *(const bool *)ctx;
    if (!quiet) {
        printf("c Model #%llu\n", (unsigned long long)idx);
    }
    printf("v ");
    for (Var v = 1; v <= num_vars; ++v) {
        printf("%s%u ", model[v] == TRUE ? "" : "-", v);
    }
    printf("0\n");
    return true;
}

static int cli_progress(void *state) {
    if(print_stats_requested) {print_stats_requested=0;print_progress_stats(state);}
    return 0;
}

/*********************************************************************
 * Usage Information
 *********************************************************************/

static void print_usage(const char* program) {
    printf("Usage: %s [OPTIONS] <input.cnf>\n", program);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help                Show this help message\n");
    printf("  -v, --verbose             Verbose runtime diagnostics (same as BSAT_VERBOSE=1)\n");
    printf("      --debug               Debug output (same as DEBUG_CDCL=1)\n");
    printf("  -q, --quiet               Suppress all output except result\n");
    printf("  -s, --stats               Print statistics (default)\n");
    printf("\n");
    printf("Resource limits:\n");
    printf("  -c, --conflicts <n>       Maximum number of conflicts\n");
    printf("  -d, --decisions <n>       Maximum number of decisions\n");
    printf("  -t, --time <sec>          Time limit in seconds\n");
    printf("  -j, --threads <n>         Parallel clause-sharing portfolio threads (default: 1)\n");
    printf("\n");
    printf("VSIDS parameters:\n");
    printf("  --var-decay <f>           Variable activity decay (default: 0.95)\n");
    printf("  --var-inc <f>             Variable activity increment (default: 1.0)\n");
    printf("\n");
    printf("Restart parameters:\n");
    printf("  --reuse-trail             Reuse high-priority restart prefixes (default: on)\n");
    printf("  --no-reuse-trail          Disable restart trail reuse\n");
    printf("  --chrono                  Enable chronological backtracking (default: on)\n");
    printf("  --no-chrono               Disable chronological backtracking\n");
    printf("  --chrono-levels <n>       Maximum ordinary jump before chronological backtracking (100)\n");
    printf("  --restart-first <n>       First restart interval (default: 100)\n");
    printf("  --restart-inc <f>         Restart multiplier (default: 1.5)\n");
    printf("  --glucose-restart         Use Glucose adaptive restarts (EMA mode)\n");
    printf("  --glucose-restart-ema     Use LBD exponential moving averages (default)\n");
    printf("  --glucose-restart-avg     Use recent-window and cumulative LBD averages\n");
    printf("  --luby-restart            Use Luby restart sequence instead of Glucose\n");
    printf("  --no-luby-restart         Disable Luby restarts (re-enable Glucose)\n");
    printf("  --luby-unit <n>           Base unit for Luby restarts (default: 100)\n");
    printf("  --no-restarts             Disable restarts\n");
    printf("\n");
    printf("Glucose tuning (EMA and sliding-window modes):\n");
    printf("  --glucose-min-conflicts <n>  Minimum conflicts since last restart (default: 2)\n");
    printf("  --glucose-k <f>           Threshold multiplier (default: 0.909)\n");
    printf("Glucose EMA tuning:\n");
    printf("  --glucose-fast-alpha <f>  Fast MA decay factor (default: 0.970)\n");
    printf("  --glucose-slow-alpha <f>  Slow MA decay factor (default: 0.99999)\n");
    printf("\n");
    printf("Glucose AVG tuning (only with --glucose-restart-avg):\n");
    printf("  --glucose-window-size <n> Window size for short-term average (default: 50)\n");
    printf("\n");
    printf("Branching heuristic:\n");
    printf("  --vmtf                    Use move-to-front variable queue in all modes\n");
    printf("  --lrb                     Use LRB/CHB instead of VSIDS\n");
    printf("  --vsids                   Use VSIDS (default in stable mode)\n");
    printf("\n");
    printf("Phase saving:\n");
    printf("  --no-phase-saving         Disable phase saving\n");
    printf("  --random-phase            Enable random phase selection (default: off)\n");
    printf("  --no-random-phase         Disable random phase selection\n");
    printf("  --random-prob <f>         Probability when random phases are enabled (0.01)\n");
    printf("  --no-rephase              Disable target phase rephasing\n");
    printf("  --rephase-interval <n>    Conflicts between rephases (default: 1000)\n");
    printf("\n");
    printf("Clause management:\n");
    printf("  --max-lbd <n>             Max LBD for keeping clauses (default: 30)\n");
    printf("  --glue-lbd <n>            LBD threshold for glue clauses (default: 2)\n");
    printf("  --reduce-fraction <f>     Fraction of clauses to keep (default: 0.5)\n");
    printf("  --reduce-interval <n>     Conflicts between reductions (default: 1000)\n");
    printf("  --reduce-increment <n>    Grow interval after each reduction (default: 100)\n");
    printf("  --binary-minimize         Binary-resolution minimization (default: on)\n");
    printf("  --no-binary-minimize      Disable binary-resolution minimization\n");
    printf("  --iterative-minimize      Binary-aware DFS minimization (always active when minimizing)\n");
    printf("  --protect-used            Temporarily retain used learned clauses (default: on)\n");
    printf("  --no-protect-used         Disable used-clause retention\n");
    printf("  --dynamic-lbd             Refresh learned-clause LBD during analysis (default: on)\n");
    printf("  --no-dynamic-lbd          Disable dynamic LBD updates\n");
    printf("  --minimize-budget <n>     Work cap per minimization pass (10000)\n");
    printf("  --no-minimize             Disable clause minimization\n");
    printf("  --shrink                  Enable block-level all-UIP shrinking (default: on)\n");
    printf("  --no-shrink               Disable block-level all-UIP shrinking\n");
    printf("  --no-subsumption          Disable on-the-fly subsumption\n");
    printf("\n");
    printf("Preprocessing & Modes:\n");
    printf("  --bce                     Enable bounded blocked clause elimination\n");
    printf("  --alternating             Focused (VMTF) / stable (VSIDS) mode alternation (default: on)\n");
    printf("  --no-alternating          Disable focused/stable mode alternation\n");
    printf("  --portfolio <sec>        Focused CPU slice, then fresh alternating search (experimental)\n");
    printf("  --no-circular            Disable circular replacement watch search\n");
    printf("  --seed <n>               Deterministic per-solver random seed\n");
    printf("  --preprocess-budget <n>  Literal-work budget (0 disables preprocessing)\n");
    printf("  --subsume-budget <n>     Subsumption candidate budget per conflict\n");
    printf("  --no-bce                  Disable blocked clause elimination\n");
    printf("  --elim                    [EXPERIMENTAL] Enable bounded variable elimination (BVE)\n");
    printf("  --no-elim                 Disable BVE (default)\n");
    printf("  --elim-max-occ <n>        Max occurrences for BVE (default: 10)\n");
    printf("  --elim-grow <n>           Max clause growth for BVE (default: 0)\n");
    printf("  --equiv                  Enable binary SCC substitution\n");
    printf("  --equiv-budget <n>       SCC preprocessing work budget (1000000)\n");
    printf("  --congruence             Experimental AND/XOR/ITE gate congruence\n");
    printf("  --congruence-budget <n>   Gate work budget (100000000)\n");
    printf("  --factor                 Experimental binary/ternary clause factoring\n");
    printf("  --factor-budget <n>       Factoring work budget (100000000)\n");
    printf("  --factor-max-variables <n> Fresh-variable limit (1024)\n");
    printf("  --factor-min-gain <n>      Minimum net clause saving per rectangle (1)\n");
    printf("  --sbva                    Structured Bounded Variable Addition (default: on, non-proof)\n");
    printf("  --no-sbva                 Disable Structured Bounded Variable Addition\n");
    printf("  --sym                     Exact syntactic symmetry breaking (default: on, non-proof)\n");
    printf("  --no-sym                  Disable syntactic symmetry breaking\n");
    printf("  --no-probing              Disable failed literal probing\n");
    printf("\n");
    printf("Inprocessing:\n");
    printf("  --inprocess               Enable inprocessing (default: on)\n");
    printf("  --no-inprocess            Disable inprocessing\n");
    printf("  --inprocess-interval <n>  Conflicts between inprocessing (default: 10000)\n");
    printf("\n");
    printf("Local search hybridization:\n");
    printf("  --local-search            Enable WalkSAT-style local search\n");
    printf("  --ls-interval <n>         Conflicts between local search calls (default: 5000)\n");
    printf("  --ls-max-flips <n>        Max flips per local search call (default: 100000)\n");
    printf("  --ls-save-phases          Save improved walk phases (requires phase saving)\n");
    printf("  --ls-noise <f>            WalkSAT noise parameter 0.0-1.0 (default: 0.5)\n");
    printf("\n");
    printf("Proof logging & input formats:\n");
    printf("  --proof <file>            Write DRAT proof to file\n");
    printf("  --accounting              Report intrusive phase CPU and owned capacities\n");
    printf("  --binary-proof            Use binary DRAT format (more compact)\n");
    printf("  --opb                     Parse and solve input as Pseudo-Boolean (.opb) format\n");
    printf("  --maxsat                  Parse and solve input as Weighted Partial MaxSAT (.wcnf)\n");
    printf("  --models <n|all>          Enumerate up to n (or all) distinct models (AllSAT)\n");
    printf("  --project <v1,v2,...>     Project --models enumeration onto listed variables\n");
    printf("  --backbone                Compute exact global backbone literals (SAT formulas)\n");
    printf("  --mus                     Extract minimal unsatisfiable subset of clause IDs (UNSAT)\n");
    printf("\n");
    printf("Output format:\n");
    printf("  Standard DIMACS output format\n");
    printf("  s SATISFIABLE / UNSATISFIABLE / UNKNOWN\n");
    printf("  v <literals> 0  (for SAT results)\n");
    printf("  c <comments>    (for statistics)\n");
}

/*********************************************************************
 * Option Parsing
 *********************************************************************/

static struct option long_options[] = {
    {"help",            no_argument,       0, 'h'},
    {"verbose",         no_argument,       0, 'v'},
    {"debug",           no_argument,       0, 0},
    {"quiet",           no_argument,       0, 'q'},
    {"stats",           no_argument,       0, 's'},
    {"conflicts",       required_argument, 0, 'c'},
    {"decisions",       required_argument, 0, 'd'},
    {"time",            required_argument, 0, 't'},
    {"threads",         required_argument, 0, 'j'},
    {"var-decay",       required_argument, 0, 0},
    {"var-inc",         required_argument, 0, 0},
    {"restart-first",   required_argument, 0, 0},
    {"restart-inc",     required_argument, 0, 0},
    {"glucose-restart", no_argument,       0, 0},
    {"glucose-restart-ema", no_argument,   0, 0},
    {"glucose-restart-avg", no_argument,   0, 0},
    {"luby-restart",    no_argument,       0, 0},
    {"no-luby-restart", no_argument,       0, 0},
    {"luby-unit",       required_argument, 0, 0},
    {"no-restarts",     no_argument,       0, 0},
    {"glucose-fast-alpha", required_argument, 0, 0},
    {"glucose-slow-alpha", required_argument, 0, 0},
    {"glucose-min-conflicts", required_argument, 0, 0},
    {"glucose-window-size", required_argument, 0, 0},
    {"glucose-k",       required_argument, 0, 0},
    {"reuse-trail", no_argument, 0, 0},
    {"no-reuse-trail", no_argument, 0, 0},
    {"chrono", no_argument, 0, 0},
    {"no-chrono", no_argument, 0, 0},
    {"chrono-levels", required_argument, 0, 0},
    {"vmtf", no_argument, 0, 0},
    {"lrb",             no_argument,       0, 0},
    {"vsids",           no_argument,       0, 0},
    {"no-phase-saving", no_argument,       0, 0},
    {"random-phase",    no_argument,       0, 0},
    {"no-random-phase", no_argument,       0, 0},
    {"random-prob",     required_argument, 0, 0},
    {"no-rephase",      no_argument,       0, 0},
    {"rephase-interval", required_argument, 0, 0},
    {"max-lbd",         required_argument, 0, 0},
    {"glue-lbd",        required_argument, 0, 0},
    {"reduce-fraction", required_argument, 0, 0},
    {"reduce-interval", required_argument, 0, 0},
    {"reduce-increment", required_argument, 0, 0},
    {"binary-minimize", no_argument, 0, 0},
    {"no-binary-minimize", no_argument, 0, 0},
    {"iterative-minimize", no_argument, 0, 0},
    {"protect-used", no_argument, 0, 0},
    {"no-protect-used", no_argument, 0, 0},
    {"dynamic-lbd", no_argument, 0, 0},
    {"no-dynamic-lbd", no_argument, 0, 0},
    {"minimize-budget", required_argument, 0, 0},
    {"no-minimize",     no_argument,       0, 0},
    {"shrink",          no_argument,       0, 0},
    {"no-shrink",       no_argument,       0, 0},
    {"no-subsumption",  no_argument,       0, 0},
    {"bce", no_argument, 0, 0},
    {"alternating", no_argument, 0, 0},
    {"no-alternating", no_argument, 0, 0},
    {"portfolio", required_argument, 0, 0},
    {"no-circular", no_argument, 0, 0},
    {"seed", required_argument, 0, 0},
    {"preprocess-budget", required_argument, 0, 0},
    {"subsume-budget", required_argument, 0, 0},
    {"no-bce",          no_argument,       0, 0},
    {"elim",            no_argument,       0, 0},
    {"no-elim",         no_argument,       0, 0},
    {"elim-max-occ",    required_argument, 0, 0},
    {"elim-grow",       required_argument, 0, 0},
    {"equiv",           no_argument,       0, 0},
    {"equiv-budget",    required_argument, 0, 0},
    {"congruence", no_argument, 0, 0},
    {"congruence-budget", required_argument, 0, 0},
    {"factor", no_argument, 0, 0},
    {"factor-budget", required_argument, 0, 0},
    {"factor-max-variables", required_argument, 0, 0},
    {"factor-min-gain", required_argument, 0, 0},
    {"sbva", no_argument, 0, 0},
    {"no-sbva", no_argument, 0, 0},
    {"sym", no_argument, 0, 0},
    {"no-sym", no_argument, 0, 0},
    {"no-probing",      no_argument,       0, 0},
    {"inprocess",       no_argument,       0, 0},
    {"no-inprocess",    no_argument,       0, 0},
    {"inprocess-interval", required_argument, 0, 0},
    {"local-search",    no_argument,       0, 0},
    {"ls-interval",     required_argument, 0, 0},
    {"ls-max-flips",    required_argument, 0, 0},
    {"ls-save-phases", no_argument, 0, 0},
    {"ls-noise",        required_argument, 0, 0},
    {"proof",           required_argument, 0, 0},
    {"accounting",      no_argument,       0, 0},
    {"binary-proof",    no_argument,       0, 0},
    {"opb",             no_argument,       0, 0},
    {"maxsat",          no_argument,       0, 0},
    {"models",          required_argument, 0, 0},
    {"project",         required_argument, 0, 0},
    {"backbone",        no_argument,       0, 0},
    {"mus",             no_argument,       0, 0},
    {0, 0, 0, 0}
};

/*********************************************************************
 * Main Function
 *********************************************************************/

int main(int argc, char** argv) {
    // Default options
    SolverOpts opts = default_opts();
    // Override from environment variables
    if (getenv("BSAT_VERBOSE")) {
        opts.verbose = true;
    }
    if (getenv("DEBUG_CDCL")) {
        opts.debug = true;
    }


    double portfolio_seconds = 0;
    uint32_t num_threads = 1;
    bool opb_mode = false;
    bool maxsat_mode = false;
    bool models_mode = false;
    uint64_t models_limit = 0;
    const char *project_spec = NULL;
    bool backbone_mode = false;
    bool mus_mode = false;

    // Parse command line options
    int c;
    int option_index = 0;

    while ((c = getopt_long(argc, argv, "hvqsc:d:t:j:", long_options, &option_index)) != -1) {
        if (optarg && !(c == 0 && (!strcmp(long_options[option_index].name, "proof") ||
                                   !strcmp(long_options[option_index].name, "models") ||
                                   !strcmp(long_options[option_index].name, "project")))) {
            const char *name = c == 0 ? long_options[option_index].name : "";
            bool floating = c == 't' || strstr(name, "decay") || strstr(name, "alpha") ||
                !strcmp(name,"var-inc") || !strcmp(name,"restart-inc") || !strcmp(name,"glucose-k") ||
                !strcmp(name,"random-prob") || !strcmp(name,"reduce-fraction") || !strcmp(name,"ls-noise") || !strcmp(name,"portfolio");
            char *end; errno=0;
            bool valid;
            if (floating) {
                double value=strtod(optarg,&end);
                valid=!errno && end!=optarg && !*end && isfinite(value) && value>=0;
            } else {
                unsigned long long value=strtoull(optarg,&end,10);
                valid=optarg[0]>='0' && optarg[0]<='9' && !errno && end!=optarg && !*end &&
                    (!strcmp(name,"preprocess-budget") || !strcmp(name,"equiv-budget") || !strcmp(name,"congruence-budget") || !strcmp(name,"factor-budget") || value<=UINT32_MAX);
            }
            if (!valid) { fprintf(stderr,"Error: invalid numeric argument: %s\n",optarg);return 1; }
        }
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 0;

            case 'v':
                opts.verbose = true;
                opts.quiet = false;
                break;

            case 'q':
                opts.quiet = true;
                opts.verbose = false;
                opts.stats = false;
                break;

            case 's':
                opts.stats = true;
                break;

            case 'c':
                opts.max_conflicts = (uint32_t)atol(optarg);
                break;

            case 'd':
                opts.max_decisions = (uint32_t)atol(optarg);
                break;

            case 't':
                opts.max_time = atof(optarg);
                break;

            case 'j': {
                unsigned long nt = strtoul(optarg, NULL, 10);
                if (nt == 0 || nt > 64) {
                    fprintf(stderr, "Error: --threads must be between 1 and 64\n");
                    return 1;
                }
                num_threads = (uint32_t)nt;
                break;
            }

            case 0:
                // Long option
                if (strcmp(long_options[option_index].name, "bce") == 0) {
                    opts.bce = true;
                } else if (strcmp(long_options[option_index].name, "alternating") == 0) {
                    opts.alternating = true;
                } else if (strcmp(long_options[option_index].name, "no-alternating") == 0) {
                    opts.alternating = false;
                } else if (strcmp(long_options[option_index].name, "congruence") == 0) {
                    opts.congruence = true;
                } else if (strcmp(long_options[option_index].name, "factor") == 0) {
                    opts.factor = true;
                } else if (strcmp(long_options[option_index].name, "factor-budget") == 0) {
                    opts.factor_budget = strtoull(optarg,NULL,10);
                } else if (strcmp(long_options[option_index].name, "factor-max-variables") == 0) {
                    opts.factor_max_variables = strtoul(optarg,NULL,10);
                } else if (strcmp(long_options[option_index].name, "factor-min-gain") == 0) {
                    opts.factor_min_gain = strtoul(optarg,NULL,10);
                } else if (strcmp(long_options[option_index].name, "congruence-budget") == 0) {
                    opts.congruence_budget = strtoull(optarg,NULL,10);
                } else if (strcmp(long_options[option_index].name, "portfolio") == 0) {
                    portfolio_seconds = atof(optarg);
                    if (portfolio_seconds <= 0) { fprintf(stderr,"Error: portfolio slice must be positive\n");return 1; }
                } else if (strcmp(long_options[option_index].name, "no-circular") == 0) {
                    opts.circular = false;
                } else if (strcmp(long_options[option_index].name, "seed") == 0) {
                    opts.seed = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "preprocess-budget") == 0) {
                    opts.preprocess_budget = strtoull(optarg,NULL,10);
                } else if (strcmp(long_options[option_index].name, "subsume-budget") == 0) {
                    opts.subsume_budget = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "debug") == 0) {
                    opts.debug = true;
                } else if (strcmp(long_options[option_index].name, "var-decay") == 0) {
                    opts.var_decay = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "var-inc") == 0) {
                    opts.var_inc = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "restart-first") == 0) {
                    opts.restart_first = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "restart-inc") == 0) {
                    opts.restart_inc = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "glucose-restart") == 0) {
                    opts.glucose_restart = true;
                } else if (strcmp(long_options[option_index].name, "luby-restart") == 0) {
                    opts.luby_restart = true;
                    opts.glucose_restart = false;  // Disable Glucose when Luby enabled
                } else if (strcmp(long_options[option_index].name, "no-luby-restart") == 0) {
                    opts.luby_restart = false;
                    opts.glucose_restart = true;   // Re-enable Glucose
                } else if (strcmp(long_options[option_index].name, "luby-unit") == 0) {
                    opts.luby_unit = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "no-restarts") == 0) {
                    opts.restart_first = UINT32_MAX;
                } else if (strcmp(long_options[option_index].name, "glucose-fast-alpha") == 0) {
                    opts.glucose_fast_alpha = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "glucose-slow-alpha") == 0) {
                    opts.glucose_slow_alpha = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "glucose-min-conflicts") == 0) {
                    opts.glucose_min_conflicts = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "glucose-restart-ema") == 0) {
                    opts.glucose_restart = true;
                    opts.glucose_use_ema = true;
                    opts.luby_restart = false;
                } else if (strcmp(long_options[option_index].name, "glucose-restart-avg") == 0) {
                    opts.glucose_restart = true;
                    opts.glucose_use_ema = false;
                    opts.luby_restart = false;
                } else if (strcmp(long_options[option_index].name, "glucose-window-size") == 0) {
                    opts.glucose_window_size = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "glucose-k") == 0) {
                    opts.glucose_k = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "reuse-trail") == 0) {
                    opts.reuse_trail = true;
                } else if (strcmp(long_options[option_index].name, "no-reuse-trail") == 0) {
                    opts.reuse_trail = false;
                } else if (strcmp(long_options[option_index].name, "chrono") == 0) {
                    opts.chrono = true;
                } else if (strcmp(long_options[option_index].name, "no-chrono") == 0) {
                    opts.chrono = false;
                } else if (strcmp(long_options[option_index].name, "chrono-levels") == 0) {
                    opts.chrono_levels = (uint32_t)strtoul(optarg,NULL,10);
                } else if (strcmp(long_options[option_index].name, "vmtf") == 0) {
                    opts.vmtf = true;
                } else if (strcmp(long_options[option_index].name, "lrb") == 0) {
                    opts.lrb = true;
                } else if (strcmp(long_options[option_index].name, "vsids") == 0) {
                    opts.lrb = false;
                } else if (strcmp(long_options[option_index].name, "no-phase-saving") == 0) {
                    opts.phase_saving = false;
                } else if (strcmp(long_options[option_index].name, "random-phase") == 0) {
                    opts.random_phase = true;
                } else if (strcmp(long_options[option_index].name, "no-random-phase") == 0) {
                    opts.random_phase = false;
                } else if (strcmp(long_options[option_index].name, "random-prob") == 0) {
                    opts.random_phase_prob = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "no-rephase") == 0) {
                    opts.rephase = false;
                } else if (strcmp(long_options[option_index].name, "rephase-interval") == 0) {
                    opts.rephase_interval = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "max-lbd") == 0) {
                    opts.max_lbd = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "glue-lbd") == 0) {
                    opts.glue_lbd = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "reduce-fraction") == 0) {
                    opts.reduce_fraction = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "reduce-interval") == 0) {
                    opts.reduce_interval = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "reduce-increment") == 0) {
                    opts.reduce_increment = (uint32_t)strtoul(optarg, NULL, 10);
                } else if (strcmp(long_options[option_index].name, "binary-minimize") == 0) {
                    opts.binary_minimize = true;
                } else if (strcmp(long_options[option_index].name, "no-binary-minimize") == 0) {
                    opts.binary_minimize = false;
                } else if (strcmp(long_options[option_index].name, "iterative-minimize") == 0) {
                    opts.iterative_minimize = true;
                } else if (strcmp(long_options[option_index].name, "protect-used") == 0) {
                    opts.protect_used = true;
                } else if (strcmp(long_options[option_index].name, "no-protect-used") == 0) {
                    opts.protect_used = false;
                } else if (strcmp(long_options[option_index].name, "dynamic-lbd") == 0) {
                    opts.dynamic_lbd = true;
                } else if (strcmp(long_options[option_index].name, "no-dynamic-lbd") == 0) {
                    opts.dynamic_lbd = false;
                } else if (strcmp(long_options[option_index].name, "minimize-budget") == 0) {
                    opts.minimize_budget = (uint32_t)strtoul(optarg, NULL, 10);
                } else if (strcmp(long_options[option_index].name, "no-minimize") == 0) {
                    opts.minimize = false;
                } else if (strcmp(long_options[option_index].name, "shrink") == 0) {
                    opts.shrink = true;
                } else if (strcmp(long_options[option_index].name, "no-shrink") == 0) {
                    opts.shrink = false;
                } else if (strcmp(long_options[option_index].name, "no-subsumption") == 0) {
                    opts.subsumption = false;
                } else if (strcmp(long_options[option_index].name, "no-bce") == 0) {
                    opts.bce = false;
                } else if (strcmp(long_options[option_index].name, "elim") == 0) {
                    opts.elim = true;
                } else if (strcmp(long_options[option_index].name, "no-elim") == 0) {
                    opts.elim = false;
                } else if (strcmp(long_options[option_index].name, "elim-max-occ") == 0) {
                    opts.elim_max_occ = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "elim-grow") == 0) {
                    opts.elim_grow = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "equiv") == 0) {
                    opts.equiv = true;
                } else if (strcmp(long_options[option_index].name, "equiv-budget") == 0) {
                    opts.equiv_budget = strtoull(optarg, NULL, 10);
                } else if (strcmp(long_options[option_index].name, "no-probing") == 0) {
                    opts.probing = false;
                } else if (strcmp(long_options[option_index].name, "sbva") == 0) {
                    opts.sbva = true;
                } else if (strcmp(long_options[option_index].name, "no-sbva") == 0) {
                    opts.sbva = false;
                } else if (strcmp(long_options[option_index].name, "sym") == 0) {
                    opts.sym = true;
                } else if (strcmp(long_options[option_index].name, "no-sym") == 0) {
                    opts.sym = false;
                } else if (strcmp(long_options[option_index].name, "inprocess") == 0) {
                    opts.inprocess = true;
                } else if (strcmp(long_options[option_index].name, "no-inprocess") == 0) {
                    opts.inprocess = false;
                } else if (strcmp(long_options[option_index].name, "inprocess-interval") == 0) {
                    opts.inprocess_interval = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "local-search") == 0) {
                    opts.local_search = true;
                } else if (strcmp(long_options[option_index].name, "ls-interval") == 0) {
                    opts.ls_interval = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "ls-max-flips") == 0) {
                    opts.ls_max_flips = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "ls-save-phases") == 0) {
                    opts.ls_save_phases = true;
                } else if (strcmp(long_options[option_index].name, "ls-noise") == 0) {
                    opts.ls_noise = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "proof") == 0) {
                    opts.proof_path = optarg;
                } else if (strcmp(long_options[option_index].name, "accounting") == 0) {
                    opts.accounting = true;
                } else if (strcmp(long_options[option_index].name, "binary-proof") == 0) {
                    opts.binary_proof = true;
                } else if (strcmp(long_options[option_index].name, "opb") == 0) {
                    opb_mode = true;
                } else if (strcmp(long_options[option_index].name, "maxsat") == 0) {
                    maxsat_mode = true;
                } else if (strcmp(long_options[option_index].name, "models") == 0) {
                    models_mode = true;
                    if (strcmp(optarg, "all") == 0) {
                        models_limit = 0;
                    } else {
                        char *end = NULL; errno = 0;
                        unsigned long long v = strtoull(optarg, &end, 10);
                        if (errno || end == optarg || *end != '\0') {
                            fprintf(stderr, "Error: invalid --models value: %s\n", optarg);
                            return 1;
                        }
                        models_limit = (uint64_t)v;
                    }
                } else if (strcmp(long_options[option_index].name, "project") == 0) {
                    project_spec = optarg;
                } else if (strcmp(long_options[option_index].name, "backbone") == 0) {
                    backbone_mode = true;
                } else if (strcmp(long_options[option_index].name, "mus") == 0) {
                    mus_mode = true;
                }
                break;

            case '?':
                // Unknown option
                return 1;
        }
    }

    // Check for input file
    if (optind >= argc) {
        fprintf(stderr, "Error: No input file specified\n");
        print_usage(argv[0]);
        return 1;
    }

    const char* input_file = argv[optind];
    if (!opb_mode && !maxsat_mode) {
        size_t flen = strlen(input_file);
        if (flen >= 4 && strcmp(input_file + flen - 4, ".opb") == 0) {
            opb_mode = true;
        } else if (flen >= 5 && strcmp(input_file + flen - 5, ".wcnf") == 0) {
            maxsat_mode = true;
        }
    }

    if (maxsat_mode) {
        MaxSatInstance inst;
        if (!maxsat_parse_file(&inst, input_file)) {
            fprintf(stderr, "Error parsing WCNF file: %s\n", input_file);
            return 1;
        }
        MaxSatResult res = maxsat_solve(&inst, &opts);
        int exit_code = 0;
        if (res.status == TRUE) {
            printf("%s\n", res.optimum ? "s OPTIMUM FOUND" : "s SATISFIABLE");
            if (res.model) {
                printf("v ");
                for (Var v = 1; v <= inst.num_vars; ++v) {
                    putchar(res.model[v] == TRUE ? '1' : '0');
                }
                putchar('\n');
            }
            if (opts.stats && !opts.quiet) {
                printf("c MaxSAT cost=%llu lower_bound=%llu cores=%u solves=%u\n",
                       (unsigned long long)res.cost, (unsigned long long)res.lower_bound,
                       res.cores, res.solves);
            }
            exit_code = res.optimum ? 30 : 10;
        } else if (res.status == FALSE) {
            printf("s UNSATISFIABLE\n");
            exit_code = 20;
        } else {
            printf("s UNKNOWN\n");
            exit_code = 0;
        }
        maxsat_result_free(&res);
        maxsat_instance_free(&inst);
        return exit_code;
    }

    if (opb_mode) {
        PbInstance inst;
        if (!pb_parse_opb_file(&inst, input_file)) {
            fprintf(stderr, "Error parsing OPB file: %s\n", input_file);
            return 1;
        }
        PbSolveResult res = pb_solve_opb(&inst, &opts);
        int exit_code = 0;
        if (res.status == TRUE) {
            printf("%s\n", (inst.has_objective && res.optimum) ? "s OPTIMUM FOUND" : "s SATISFIABLE");
            if (res.best_model) {
                printf("v ");
                for (Var v = 1; v <= inst.num_vars; ++v) {
                    printf("%sx%u ", res.best_model[v] == TRUE ? "" : "-", v);
                    if (v % 20 == 0 && v < inst.num_vars) printf("\nv ");
                }
                printf("\n");
            }
            exit_code = (inst.has_objective && res.optimum) ? 30 : 10;
        } else if (res.status == FALSE) {
            printf("s UNSATISFIABLE\n");
            exit_code = 20;
        } else {
            printf("s UNKNOWN\n");
            exit_code = 0;
        }
        pb_solve_result_free(&res);
        pb_instance_free(&inst);
        return exit_code;
    }

    struct stat input_stat, proof_stat;
    if (opts.proof_path && !stat(input_file,&input_stat) && !stat(opts.proof_path,&proof_stat) &&
        input_stat.st_dev==proof_stat.st_dev && input_stat.st_ino==proof_stat.st_ino) {
        fprintf(stderr,"Error: input and proof must be different files\n");return 1;
    }

    if (num_threads > 1 && opts.proof_path) {
        fprintf(stderr, "Error: --proof is not supported with --threads > 1\n");
        return 1;
    }

    // Print header
    if (!opts.quiet) {
        printf("c BSAT Competition Solver v1.0\n");
        printf("c PID: %d (send SIGUSR1 for progress: kill -USR1 %d)\n", getpid(), getpid());
        printf("c Reading from %s\n", input_file);
    }

    // Create solver
    Solver* solver = solver_new_with_opts(&opts);
    if (!solver) {
        fprintf(stderr, "Error: Failed to create solver\n");
        return 1;
    }

    // Parse input file (supports standard DIMACS CNF and CryptoMiniSat XOR-CNF "x ... 0")
    uint32_t header_vars = 0;
    DimacsError err = dimacs_parse_file_xor(solver, input_file, &header_vars);
    if (err != DIMACS_OK) {
        fprintf(stderr, "Error parsing DIMACS file: %s\n", dimacs_error_string(err));
        solver_free(solver);
        return 1;
    }

    if (!opts.quiet) {
        printf("c Variables: %u\n", solver->num_vars);
        printf("c Clauses:   %u\n", solver->num_clauses);
        printf("c\n");
    }

    if (opts.accounting && !opts.quiet) {
        puts("c Accounting checkpoint: parsed");
        solver_print_accounting(solver);
    }

    if (models_mode) {
        Var *pvars = NULL;
        uint32_t npvars = 0, pcap = 0;
        if (project_spec) {
            const char *p = project_spec;
            while (*p) {
                while (*p == ',' || *p == ' ') p++;
                if (!*p) break;
                char *end = NULL; errno = 0;
                unsigned long v = strtoul(p, &end, 10);
                if (errno || end == p || v == 0 || v > solver->num_vars || (*end && *end != ',' && *end != ' ')) {
                    fprintf(stderr, "Error: invalid variable in --project: %s\n", project_spec);
                    free(pvars); solver_free(solver); return 1;
                }
                if (npvars == pcap) {
                    uint32_t nc = pcap ? pcap * 2u : 16u;
                    Var *nv = realloc(pvars, (size_t)nc * sizeof *nv);
                    if (!nv) { free(pvars); solver_free(solver); return 1; }
                    pvars = nv; pcap = nc;
                }
                pvars[npvars++] = (Var)v;
                p = end;
            }
        }
        bool qflag = opts.quiet;
        AllSatResult ar = bsat_enumerate_models(solver, header_vars ? header_vars : solver->num_vars,
                                                pvars, npvars, models_limit, &opts,
                                                cli_print_model_cb, &qflag);
        free(pvars);
        if (ar.status == TRUE) {
            printf("s SATISFIABLE\n");
            printf("c Models enumerated: %llu (%s, solves=%u)\n",
                   (unsigned long long)ar.count, ar.complete ? "complete" : "partial", ar.solves);
            solver_free(solver);
            return 10;
        } else if (ar.status == FALSE) {
            printf("s UNSATISFIABLE\n");
            solver_free(solver);
            return 20;
        }
        printf("s UNKNOWN\n");
        solver_free(solver);
        return 0;
    }

    if (backbone_mode) {
        BackboneResult br = bsat_compute_backbone(solver, header_vars ? header_vars : solver->num_vars, &opts);
        int rc = 0;
        if (br.status == TRUE) {
            printf("s SATISFIABLE\n");
            printf("b ");
            for (uint32_t i = 0; i < br.size; ++i) printf("%d ", toDimacs(br.lits[i]));
            printf("0\n");
            if (!opts.quiet) {
                printf("c Backbone size: %u (%s, solves=%u, flip_dropped=%u)\n",
                       br.size, br.complete ? "exact" : "partial", br.solves, br.flip_dropped);
            }
            rc = 10;
        } else if (br.status == FALSE) {
            printf("s UNSATISFIABLE\n");
            rc = 20;
        } else {
            printf("s UNKNOWN\n");
        }
        bsat_backbone_result_free(&br);
        solver_free(solver);
        return rc;
    }

    if (mus_mode) {
        MusResult mr = bsat_compute_mus(solver, header_vars ? header_vars : solver->num_vars, &opts);
        int rc = 0;
        if (mr.status == FALSE) {
            printf("s UNSATISFIABLE\n");
            printf("m ");
            for (uint32_t i = 0; i < mr.size; ++i) printf("%u ", mr.clause_ids[i] + 1u); /* 1-based clause IDs */
            printf("0\n");
            if (!opts.quiet) {
                printf("c MUS size: %u / %u clauses (%s, solves=%u, rotated=%u)\n",
                       mr.size, mr.orig_clauses, mr.minimal ? "minimal" : "partial", mr.solves, mr.rotated);
            }
            rc = 20;
        } else if (mr.status == TRUE) {
            printf("s SATISFIABLE\n");
            rc = 10;
        } else {
            printf("s UNKNOWN\n");
        }
        bsat_mus_result_free(&mr);
        solver_free(solver);
        return rc;
    }

    // The executable owns its progress signal; the library never installs it.
    install_signal_handlers();
    solver_set_terminate(solver,solver,cli_progress);
    // Solve
    double start_time = (double)clock() / CLOCKS_PER_SEC;
    lbool result = (num_threads > 1)
        ? solver_solve_parallel(solver, num_threads)
        : (portfolio_seconds ? solver_solve_portfolio(solver, portfolio_seconds) : solver_solve(solver));
    double solve_time = (double)clock() / CLOCKS_PER_SEC - start_time;

    if (solver->error) fprintf(stderr, "Error: solver allocation, certificate, or model validation failure\n");
    // Print result
    if (result == TRUE) {
        printf("s SATISFIABLE\n");

        // Print model
        printf("v ");
        int vars_per_line = 0;
        Var model_vars = solver->factor_original_vars ? solver->factor_original_vars : solver->num_vars;
        if (header_vars && header_vars < model_vars) model_vars = header_vars;
        for (Var v = 1; v <= model_vars; v++) {
            lbool val = solver_model_value(solver, v);
            if (val == TRUE) {
                printf("%u ", v);
            } else if (val == FALSE) {
                printf("-%u ", v);
            } else {
                // Variable doesn't appear in formula - set to false
                printf("-%u ", v);
            }

            vars_per_line++;
            if (vars_per_line >= 20) {
                printf("\nv ");
                vars_per_line = 0;
            }
        }
        printf("0\n");
    } else if (result == FALSE) {
        printf("s UNSATISFIABLE\n");
    } else {
        printf("s UNKNOWN\n");
    }

    // Print statistics
    if (opts.stats && !opts.quiet) {
        printf("c\n");
        printf("c CPU time:         %.3f s\n", solve_time);
        solver_print_stats(solver);
        if (opts.accounting) {
            puts("c Accounting checkpoint: final");
            solver_print_accounting(solver);
        }
    }

    // Clean up
    bool failed = solver->error;
    solver_free(solver);
    if (failed) return 1;

    return (result == TRUE || result == FALSE) ? 10 + (result == TRUE ? 0 : 10) : 0;
}
