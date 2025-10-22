/*********************************************************************
 * BSAT Competition Solver - Main Entry Point
 *********************************************************************/

#include "../include/solver.h"
#include "../include/dimacs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>

/*********************************************************************
 * Usage Information
 *********************************************************************/

static void print_usage(const char* program) {
    printf("Usage: %s [OPTIONS] <input.cnf>\n", program);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help                Show this help message\n");
    printf("  -v, --verbose             Verbose output\n");
    printf("  -q, --quiet               Suppress all output except result\n");
    printf("  -s, --stats               Print statistics (default)\n");
    printf("\n");
    printf("Resource limits:\n");
    printf("  -c, --conflicts <n>       Maximum number of conflicts\n");
    printf("  -d, --decisions <n>       Maximum number of decisions\n");
    printf("  -t, --time <sec>          Time limit in seconds\n");
    printf("\n");
    printf("VSIDS parameters:\n");
    printf("  --var-decay <f>           Variable activity decay (default: 0.95)\n");
    printf("  --var-inc <f>             Variable activity increment (default: 1.0)\n");
    printf("\n");
    printf("Restart parameters:\n");
    printf("  --restart-first <n>       First restart interval (default: 100)\n");
    printf("  --restart-inc <f>         Restart multiplier (default: 1.5)\n");
    printf("  --glucose-restart         Use Glucose adaptive restarts\n");
    printf("  --no-restarts             Disable restarts\n");
    printf("\n");
    printf("Glucose tuning (only with --glucose-restart):\n");
    printf("  --glucose-fast-alpha <f>  Fast MA decay factor (default: 0.8)\n");
    printf("  --glucose-slow-alpha <f>  Slow MA decay factor (default: 0.9999)\n");
    printf("  --glucose-min-conflicts <n>  Min conflicts before Glucose (default: 100)\n");
    printf("\n");
    printf("Phase saving:\n");
    printf("  --no-phase-saving         Disable phase saving\n");
    printf("  --random-phase            Enable random phase selection\n");
    printf("  --random-prob <f>         Random phase probability (default: 0.01)\n");
    printf("\n");
    printf("Clause management:\n");
    printf("  --max-lbd <n>             Max LBD for keeping clauses (default: 30)\n");
    printf("  --glue-lbd <n>            LBD threshold for glue clauses (default: 2)\n");
    printf("  --reduce-fraction <f>     Fraction of clauses to keep (default: 0.5)\n");
    printf("  --reduce-interval <n>     Conflicts between reductions (default: 2000)\n");
    printf("\n");
    printf("Preprocessing:\n");
    printf("  --bce                     Enable blocked clause elimination (default)\n");
    printf("  --no-bce                  Disable blocked clause elimination\n");
    printf("\n");
    printf("Inprocessing:\n");
    printf("  --inprocess               Enable inprocessing (vivification, etc.)\n");
    printf("  --inprocess-interval <n>  Conflicts between inprocessing (default: 10000)\n");
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
    {"quiet",           no_argument,       0, 'q'},
    {"stats",           no_argument,       0, 's'},
    {"conflicts",       required_argument, 0, 'c'},
    {"decisions",       required_argument, 0, 'd'},
    {"time",            required_argument, 0, 't'},
    {"var-decay",       required_argument, 0, 0},
    {"var-inc",         required_argument, 0, 0},
    {"restart-first",   required_argument, 0, 0},
    {"restart-inc",     required_argument, 0, 0},
    {"glucose-restart", no_argument,       0, 0},
    {"no-restarts",     no_argument,       0, 0},
    {"glucose-fast-alpha", required_argument, 0, 0},
    {"glucose-slow-alpha", required_argument, 0, 0},
    {"glucose-min-conflicts", required_argument, 0, 0},
    {"no-phase-saving", no_argument,       0, 0},
    {"random-phase",    no_argument,       0, 0},
    {"random-prob",     required_argument, 0, 0},
    {"max-lbd",         required_argument, 0, 0},
    {"glue-lbd",        required_argument, 0, 0},
    {"reduce-fraction", required_argument, 0, 0},
    {"reduce-interval", required_argument, 0, 0},
    {"bce",             no_argument,       0, 0},
    {"no-bce",          no_argument,       0, 0},
    {"inprocess",       no_argument,       0, 0},
    {"inprocess-interval", required_argument, 0, 0},
    {0, 0, 0, 0}
};

/*********************************************************************
 * Main Function
 *********************************************************************/

int main(int argc, char** argv) {
    // Default options
    SolverOpts opts = default_opts();

    // Parse command line options
    int c;
    int option_index = 0;

    while ((c = getopt_long(argc, argv, "hvqsc:d:t:", long_options, &option_index)) != -1) {
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

            case 0:
                // Long option
                if (strcmp(long_options[option_index].name, "var-decay") == 0) {
                    opts.var_decay = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "var-inc") == 0) {
                    opts.var_inc = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "restart-first") == 0) {
                    opts.restart_first = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "restart-inc") == 0) {
                    opts.restart_inc = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "glucose-restart") == 0) {
                    opts.glucose_restart = true;
                } else if (strcmp(long_options[option_index].name, "no-restarts") == 0) {
                    opts.restart_first = UINT32_MAX;
                } else if (strcmp(long_options[option_index].name, "glucose-fast-alpha") == 0) {
                    opts.glucose_fast_alpha = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "glucose-slow-alpha") == 0) {
                    opts.glucose_slow_alpha = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "glucose-min-conflicts") == 0) {
                    opts.glucose_min_conflicts = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "no-phase-saving") == 0) {
                    opts.phase_saving = false;
                } else if (strcmp(long_options[option_index].name, "random-phase") == 0) {
                    opts.random_phase = true;
                } else if (strcmp(long_options[option_index].name, "random-prob") == 0) {
                    opts.random_phase_prob = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "max-lbd") == 0) {
                    opts.max_lbd = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "glue-lbd") == 0) {
                    opts.glue_lbd = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "reduce-fraction") == 0) {
                    opts.reduce_fraction = atof(optarg);
                } else if (strcmp(long_options[option_index].name, "reduce-interval") == 0) {
                    opts.reduce_interval = (uint32_t)atol(optarg);
                } else if (strcmp(long_options[option_index].name, "bce") == 0) {
                    opts.bce = true;
                } else if (strcmp(long_options[option_index].name, "no-bce") == 0) {
                    opts.bce = false;
                } else if (strcmp(long_options[option_index].name, "inprocess") == 0) {
                    opts.inprocess = true;
                } else if (strcmp(long_options[option_index].name, "inprocess-interval") == 0) {
                    opts.inprocess_interval = (uint32_t)atol(optarg);
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

    // Parse input file
    DimacsError err = dimacs_parse_file(solver, input_file);
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

    // Solve
    double start_time = (double)clock() / CLOCKS_PER_SEC;
    lbool result = solver_solve(solver);
    double solve_time = (double)clock() / CLOCKS_PER_SEC - start_time;

    // Print result
    if (result == TRUE) {
        printf("s SATISFIABLE\n");

        // Print model
        printf("v ");
        int vars_per_line = 0;
        for (Var v = 1; v <= solver->num_vars; v++) {
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
    }

    // Clean up
    solver_free(solver);

    return (result == TRUE || result == FALSE) ? 10 + (result == TRUE ? 0 : 10) : 0;
}