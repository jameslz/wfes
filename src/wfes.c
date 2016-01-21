#define _POSIX_C_SOURCE 200809L
#include "dkl.h"

void print_help() {
    printf( "WFES: Wright-Fisher model solver\n"
            "USAGE:\n"
            "-N, --population_size:        Population size\n"
            "-s, --selection_coefficient:  Selection coefficient\n"
            "-u, --forward_mutation_rate:  Mutation rate from a to A\n"
            "-v, --backward_mutation_rate: Mutation rate from A to a\n"
            "-d, --dominance_coefficient:  Proportion of selection Aa recieves\n");
}

/**
 * wfes: solve for conditional time to absorption and sojourn times
 *
 * Input parameters
 * @param[in] N Population size (range: 2-Inf)
 *   The effective population size
 * @param[in] s Selection coefficient (range: -1:Inf)
 *   The relative selective advantage of allele 'A' over allele 'a'
 * @param[in] u Forward mutation rate (range: 0:1/2N)
 *   The rate of mutation from 'A' into 'a'
 * @param[in] v Backward mutation rate (range: 0:1/2N)
 *   The rate of mutation from 'a' into 'A'
 * @param[in] h Dominance coefficient (range: 0:1)
 *   The proportion of selective advantage a heterozygote 'Aa' caries
 * @param[in] t Numeric zero threshold (range: 0:1e-10)
 *   Any number below 't' is considered zero
 *
 * @param[out] B1 Probability of extinction vector (size: 2N-1)
 *   B1[i] is the probability of extinction, given the population starts with i+1 copies of A
 * @param[out] B2 Probability of fixation vector (size: 2N-1)
 *   B2[i] is the probability of fixation, given the population starts with i+1 copies of A
 * @param[out] N Sojourn time vector (size: 2N-1)
 *  N[i] is the number of generations the population is expected to spend with i+1 copies of A, given the population starts with 1 copy of A
 *
 */
void wfes(wf_parameters *wf, wf_statistics *r, double zero_threshold) {
  // Declaration
  DKL_INT matrix_size = (2 * wf->population_size) - 1;
  DKL_INT mtype = 11;
  DKL_INT nrhs = 1;
  void *pt[64];

  // Pardiso control parameters
  DKL_INT iparm[64];
  DKL_INT maxfct, mnum, phase, error, msglvl;

  // Auxiliary variables
  DKL_INT i;
  double ddum;  // double dummy
  DKL_INT idum; // integer dummy

  double *rhs = dkl_alloc(matrix_size, double);
  double *workspace = dkl_alloc(matrix_size, double);

  DKL_INT block_size;
  if (matrix_size >= 100) {
    block_size = matrix_size * 0.1;
  } else {
    block_size = matrix_size;
  }

  double start_time, end_time, global_start_time, global_end_time;

#ifdef DEBUG
  MKL_Peak_Mem_Usage(MKL_PEAK_MEM_ENABLE);
  global_start_time = get_current_time();
  start_time = get_current_time();
#endif

  csr_sparse_matrix *A = wf_matrix_csr(wf, block_size, zero_threshold);

#ifdef DEBUG
  assert(csr_sparse_is_correct(A) == true);
  end_time = get_current_time();
  printf("Building matrix: %gs\n", end_time - start_time);
#endif

  for (i = 0; i < matrix_size; i++) {
    double q = wf_sampling_coefficient(wf, i + 1);
    rhs[i] = pow(1 - q, 2 * wf->population_size);
  }

  // Setup Pardiso control parameters
  for (i = 0; i < 64; i++) {
    iparm[i] = 0;
  }

  iparm[0] = 1; // No solver default
  iparm[1] = 3; // Fill-in reordering from METIS
  iparm[2] = 1;
  iparm[3] = 0;   // No iterative-direct algorithm
  iparm[4] = 0;   // No user fill-in reducing permutation
  iparm[5] = 1;   // Don't write solution into x
  iparm[6] = 0;   // Not in use
  iparm[7] = 2;   // Max numbers of iterative refinement steps
  iparm[8] = 0;   // Not in use
  iparm[9] = 20;  // Perturb the pivot elements with 1E-20
  iparm[10] = 1;  // Use nonsymmetric permutation and scaling MPS
  iparm[11] = 0;  // Conjugate transposed/transpose solve
  iparm[12] = 1;  // Maximum weighted matching algorithm is switched-on
  iparm[13] = 0;  // Output: Number of perturbed pivots
  iparm[14] = 0;  // Not in use
  iparm[15] = 0;  // Not in use
  iparm[16] = 0;  // Not in use
  iparm[17] = -1; // Output: Number of nonzeros in the factor LU
  iparm[18] = -1; // Output: Mflops for LU factorization
  iparm[19] = 0;  // Output: Numbers of CG Iterations
  iparm[26] = 0;  // Double precision
  iparm[34] = 1;
  iparm[59] = 1;

#ifdef DEBUG
  msglvl = 0; // Print statistical information
#else
  msglvl = 0; // Print statistical information in file
#endif

  maxfct = 1; // Maximum number of numerical factorizations.
  mnum = 1;   // Which factorization to use.
  error = 0;  // Initialize error flag

  for (i = 0; i < 64; i++) {
    pt[i] = 0;
  }
  //}}}1

  // Symbolic Factorization {{{1
  phase = 11;
#ifdef DEBUG
  start_time = get_current_time();
#endif
  pardiso_64(pt, &maxfct, &mnum, &mtype, &phase, &matrix_size, A->data,
             A->row_index, A->cols, &idum, &nrhs, iparm, &msglvl, &ddum, &ddum,
             &error);
  if (error != 0) {
    printf("ERROR during symbolic factorization: %" PRId64 "\n", error);
    exit(phase);
  }
#ifdef DEBUG
  end_time = get_current_time();
  printf("Symbolic factorization %gs\n", end_time - start_time);
#endif
  //}}}1

  // Numeric factorization {{{1
  phase = 22;
#ifdef DEBUG
  start_time = get_current_time();
#endif
  pardiso_64(pt, &maxfct, &mnum, &mtype, &phase, &matrix_size, A->data,
             A->row_index, A->cols, &idum, &nrhs, iparm, &msglvl, &ddum, &ddum,
             &error);
  if (error != 0) {
    printf("ERROR during numeric factorization: %" PRId64 "\n", error);
    exit(phase);
  }
#ifdef DEBUG
  end_time = get_current_time();
  printf("Numeric factorization %gs\n", end_time - start_time);
#endif
  //}}}1

  // Solution {{{1
  phase = 33;

  iparm[11] = 0;
#ifdef DEBUG
  start_time = get_current_time();
#endif
  pardiso_64(pt, &maxfct, &mnum, &mtype, &phase, &matrix_size, A->data,
             A->row_index, A->cols, &idum, &nrhs, iparm, &msglvl, rhs,
             workspace, &error);
  if (error != 0) {
    printf("ERROR during solution: %" PRId64 "\n", error);
    exit(phase);
  } else {
    memcpy(r->B1, rhs, matrix_size * sizeof(double));
    for (i = 0; i < matrix_size; i++) {
      if (r->B1[i] < 0) {
        r->B1[i] = 0.0;
      }
      r->B2[i] = 1.0 - r->B1[i];
    }
  }

  println("Solution successful!");
  rhs[0] = 1.0;
  for (i = 1; i < matrix_size; i++) {
    rhs[i] = 0.0;
  }
  iparm[11] = 2;
  pardiso_64(pt, &maxfct, &mnum, &mtype, &phase, &matrix_size, A->data,
             A->row_index, A->cols, &idum, &nrhs, iparm, &msglvl, rhs,
             workspace, &error);
  if (error != 0) {
    printf("ERROR during solution: %" PRId64 "\n", error);
    exit(phase);
  } else {
    memcpy(r->N, rhs, matrix_size * sizeof(double));
    for (i = 0; i < matrix_size; i++) {
      if (r->N[i] < 0) {
        r->N[i] = 0;
      }
    }
  }
#ifdef DEBUG
  end_time = get_current_time();
  printf("Solution %gs\n", end_time - start_time);
#endif
  //}}}1

  // Calculate the summary statistics
  for (i = 0; i < matrix_size; i++) {
    r->time_extinction += (r->B1[i] * r->N[i]);
    r->time_fixation += (r->B2[i] * r->N[i]);
    r->count_before_extinction += (r->N[i] * r->B1[i]) * (i + 1);
  }
  r->time_extinction /= r->B1[0];
  r->time_fixation /= r->B2[0];

  r->count_before_extinction /= r->B1[0];

  if (r->B1[0] <= 0) {
    r->probability_extinction = 0;
    r->time_extinction = NAN;
  } else {
    r->probability_extinction = r->B1[0];
  }
  if (r->B2[0] <= 0) {
    r->probability_fixation = 0;
    r->time_fixation = NAN;
  } else {
    r->probability_fixation = r->B2[0];
  }

  // Memory release {{{1
  phase = -1; // Release internal memory.
  pardiso_64(pt, &maxfct, &mnum, &mtype, &phase, &matrix_size, &ddum,
             A->row_index, A->cols, &idum, &nrhs, iparm, &msglvl, &ddum, &ddum,
             &error);

  dkl_dealloc(rhs);
  dkl_dealloc(workspace);

  dkl_dealloc(A->data);
  dkl_dealloc(A->cols);
  dkl_dealloc(A->row_index);
  dkl_dealloc(A);
//}}}1

#ifdef DEBUG
  printf("Memory used: %.3g GB\n",
         (double)MKL_Peak_Mem_Usage(MKL_PEAK_MEM) / (GB_CONV));
  global_end_time = get_current_time();
  printf("Total runtime %gs\n", global_end_time - global_start_time);
#endif
}

/**
* main
*/
int main(int argc, char **argv) {
    // Parse the command line
    bool help = dkl_args_parse_flag(argc, argv, false, "h", "-h", "--h", "help", "-help", "--help", NULL);
    if (help) {
        print_help();
        exit(DKL_HELP_EXIT);
    }

    // Initialize WF input parameters
    wf_parameters *wf = dkl_new(wf_parameters);

    // Parse the required arguments
    wf->population_size = dkl_args_parse_int(argc, argv, true, "n", "-n", "--n", "N", "-N", "--N", "population_size", "-population_size", "--population_size", NULL);
    wf->selection = dkl_args_parse_double(argc, argv, true, "s", "-s", "--s", "selection_coefficient", "-selection_coefficient", "--selection_coefficient", NULL);
    wf->forward_mutation_rate = dkl_args_parse_double(argc, argv, true, "u", "-u", "--u", "forward_mutation_rate", "-forward_mutation_rate", "--forward_mutation_rate", NULL);
    wf->backward_mutation_rate = dkl_args_parse_double(argc, argv, true, "v", "-v", "--v", "backward_mutation_rate", "-backward_mutation_rate", "--backward_mutation_rate", NULL);
    wf->dominance_coefficient = dkl_args_parse_double(argc, argv, true, "d", "-d", "--d", "dominance_coefficient", "-dominance_coefficient", "--dominance_coefficient", NULL);

    // Parse optional parameters
    double zero_threshold = dkl_args_parse_double(argc, argv, false, "z", "-z", "--z", "zero_threshold", "-zero_threshold", "--zero_threshold", NULL);
    if (dkl_errno == DKL_OPTION_NOT_FOUND) {
        zero_threshold = 1e-30;
        dkl_clear_errno();
    }

    char *generations_file = dkl_args_parse_string(argc, argv, false, "g", "-g", "--g", "generations_file",
    "-generations_file", "--generations_file", "sojourn_time_file", "-sojourn_time_file", "--sojourn_time_file", NULL);
    char *extinction_file = dkl_args_parse_string(argc, argv, false, "e", "-e", "--e", "extinction_file",
"-extinction_file", "--extinction_file", NULL);
    char *fixation_file = dkl_args_parse_string(argc, argv, false, "f", "-f", "--f", "fixation_file",
"-fixation_file", "--fixation_file", NULL);


    bool force = dkl_args_parse_flag(argc, argv, false, "force", "-force", "--force", NULL);
    if (!force) {
        if (wf->population_size > 500000) {
            error_print("The population_size parameter is too large - the computation might take a very long time");
            println("Use `--force` to override");
            exit(DKL_PARAM_ERROR);
        }
        double max_mutation_rate = 1.0 / (2.0 * wf->population_size);
        if (wf->forward_mutation_rate > max_mutation_rate || wf->backward_mutation_rate > max_mutation_rate) {
            error_print("The mutation rate might violate the Wright-Fisher assumptions");
            println("Use `--force` to override");
            exit(DKL_PARAM_ERROR);
        }
    }


    DKL_INT matrix_size = (2 * wf->population_size) - 1;

    wf_statistics *results = wf_statistics_new(wf->population_size);

    wfes(wf, results, zero_threshold);

    // Output the results
    printf("%" PRId64 ",%g,%g,%g,%g,%g,%g,%g,%g,%g\n",
    wf->population_size, wf->selection, wf->forward_mutation_rate, wf->backward_mutation_rate, wf->dominance_coefficient,
    results->probability_extinction, results->probability_fixation, results->time_extinction, results->time_fixation, results->count_before_extinction);

    if (generations_file) {
        FILE *f = fopen(generations_file, "w");
        if (f != NULL) {
            for (DKL_INT i = 0; i < matrix_size - 1; i++) {
                fprintf(f, "%g,", results->N[i]);
            }
            fprintf(f, "%g\n", results->N[matrix_size - 1]);
            fclose(f);
        }
    }
    if (extinction_file) {
        FILE *f = fopen(extinction_file, "w");
        if (f != NULL) {
            for (DKL_INT i = 0; i < matrix_size - 1; i++) {
                fprintf(f, "%g,", results->B1[i]);
            }
            fprintf(f, "%g\n", results->B1[matrix_size - 1]);
            fclose(f);
        }
    }
    if (fixation_file) {
        FILE *f = fopen(fixation_file, "w");
        if (f != NULL) {
            for (DKL_INT i = 0; i < matrix_size - 1; i++) {
                fprintf(f, "%g,", results->B2[i]);
            }
            fprintf(f, "%g\n", results->B2[matrix_size - 1]);
            fclose(f);
        }
    }


    // Deallocation
    dkl_del(wf);
    wf_statistics_del(results);

    return 0;
}
