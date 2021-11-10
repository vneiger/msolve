/* This file is part of msolve.
 *
 * msolve is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * msolve is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with msolve.  If not, see <https://www.gnu.org/licenses/>
 *
 * Authors:
 * Jérémy Berthomieu
 * Christian Eder
 * Mohab Safey El Din */


#include "f4.h"

/* The parameters themselves are handled by julia, thus we only
 * free what they are pointing to, julia's garbage collector then
 * takes care of everything leftover. */
void free_f4_julia_result_data(
        void (*freep) (void *),
        int32_t **blen, /* length of each poly in basis */
        int32_t **bexp, /* basis exponent vectors */
        void **bcf,      /* coefficients of basis elements */
        const int64_t ngens,
        const int64_t field_char
        )
{
    int64_t i;
    int64_t len = 0;

    /* lengths resp. nterms */
    int32_t *lens  = *blen;
    for (i = 0; i < ngens; ++i) {
        len += (int64_t)lens[i];
    }

    (*freep)(lens);
    lens = NULL;
    *blen = lens;

    /* exponent vectors */
    int32_t *exps = *bexp;
    (*freep)(exps);
    exps  = NULL;
    *bexp = exps;

    /* coefficients */
    if (field_char == 0) {
        mpz_t **cfs = (mpz_t **)bcf;
        for (i = 0; i < len; ++i) {
            mpz_clear((*cfs)[i]);
        }
        (*freep)(*cfs);
        (*freep)(cfs);
        cfs = NULL;
    } else {
        if (field_char > 0) {
            int32_t *cfs  = *((int32_t **)bcf);
            (*freep)(cfs);
            cfs = NULL;
        }
    }
    *bcf  = NULL;
}

static void clear_matrix(
        mat_t *mat
        )
{
    len_t i;
    for (i = 0; i < mat->rbal; ++i) {
        free(mat->rba[i]);
    }
    free(mat->rba);
    mat->rba  = NULL;
    free(mat->rr);
    mat->rr = NULL;
    free(mat->tr);
    mat->tr  = NULL;
    free(mat->cf_8);
    mat->cf_8 = NULL;
    free(mat->cf_16);
    mat->cf_16  = NULL;
    free(mat->cf_32);
    mat->cf_32  = NULL;
    free(mat->cf_qq);
    mat->cf_qq  = NULL;
    free(mat->cf_ab_qq);
    mat->cf_ab_qq  = NULL;
}

static void reduce_basis(
        bs_t *bs,
        mat_t *mat,
        hi_t **hcmp,
        ht_t **bhtp,
        ht_t **shtp,
        stat_t *st
        )
{
    /* timings */
    double ct0, ct1, rt0, rt1;
    ct0 = cputime();
    rt0 = realtime();

    len_t i, j, k;

    ht_t *bht   = *bhtp;
    ht_t *sht   = *shtp;
    hi_t *hcm   = *hcmp;
    exp_t *etmp = bht->ev[0];
    memset(etmp, 0, (unsigned long)(bht->evl) * sizeof(exp_t));

    mat->rr = (hm_t **)malloc((unsigned long)bs->lml * 2 * sizeof(hm_t *));
    mat->nr = 0;
    mat->sz = 2 * bs->lml;

    /* add all non-redundant basis elements as matrix rows */
    for (i = 0; i < bs->lml; ++i) {
        mat->rr[mat->nr] = multiplied_poly_to_matrix_row(
                sht, bht, 0, etmp, bs->hm[bs->lmps[i]]);
        sht->hd[mat->rr[mat->nr][OFFSET]].idx  = 1;
        mat->nr++;
    }
    mat->nc = mat->nr; /* needed for correct counting in symbol */
    symbolic_preprocessing(mat, bs, st, sht, NULL, bht);
    /* no known pivots, we need mat->ncl = 0, so set all indices to 1 */
    for (i = 0; i < sht->eld; ++i) {
        sht->hd[i].idx = 1;
    }

    /* free data from bht, we use sht later on */
    free_hash_table(&bht);

    /* generate hash <-> column mapping */
    if (st->info_level > 1) {
        printf("reduce final basis ");
        fflush(stdout);
    }
    convert_hashes_to_columns(&hcm, mat, st, sht);
    mat->nc = mat->ncl + mat->ncr;
    /* sort rows */
    sort_matrix_rows_decreasing(mat->rr, mat->nru);
    /* do the linear algebra reduction */
    interreduce_matrix_rows(mat, bs, st);
    /* remap rows to basis elements (keeping their position in bs) */
    convert_sparse_matrix_rows_to_basis_elements_use_sht(mat, bs, hcm, st);

    /* bht becomes sht, so we do not have to convert the hash entries */
    bht   = sht;
    *bhtp = bht;

    /* set sht = NULL, otherwise we might run in a double-free
     * of sht and bht at the end */
    sht   = NULL;
    *shtp = sht;

    bs->ld  = mat->np;

    /* clean_hash_table(sht); */
    clear_matrix(mat);

    /* we may have added some multiples of reduced basis polynomials
     * from the matrix, so we get rid of them. */
    k = 0;
    i = 0;
start:
    for (; i < bs->ld; ++i) {
        for (j = 0; j < k; ++j) {
            if (check_monomial_division(
                        bs->hm[bs->ld-1-i][OFFSET],
                        bs->hm[bs->lmps[j]][OFFSET], bht)) {
                ++i;
                goto start;
            }
        }
        bs->lmps[k] = bs->ld-1-i;
        bs->lm[k++] = bht->hd[bs->hm[bs->ld-1-i][OFFSET]].sdm;
    }
    *hcmp = hcm;

    /* timings */
    ct1 = cputime();
    rt1 = realtime();
    st->reduce_gb_ctime = ct1 - ct0;
    st->reduce_gb_rtime = rt1 - rt0;
    if (st->info_level > 1) {
        printf("%13.2f sec\n", rt1-rt0);
    }

    if (st->info_level > 1) {
        printf("-------------------------------------------------\
----------------------------------------\n");
    }
}

int initialize_f4_input_data(
        bs_t **bsp,
        ht_t **bhtp,
        stat_t **stp,
        /* input values */
        const int32_t *lens,
        const int32_t *exps,
        const void *cfs,
        const uint32_t field_char,
        const int32_t mon_order,
        const int32_t elim_block_len,
        const int32_t nr_vars,
        const int32_t nr_gens,
        const int32_t ht_size,
        const int32_t nr_threads,
        const int32_t max_nr_pairs,
        const int32_t reset_ht,
        const int32_t la_option,
        const int32_t reduce_gb,
        const int32_t pbm_file,
        const int32_t info_level
        )
{
    bs_t *bs    = *bsp;
    ht_t *bht   = *bhtp;
    stat_t *st  = *stp;

    /* initialize stuff */
    st  = initialize_statistics();

    /* checks and set all meta data. if a nonzero value is returned then
     * some of the input data is corrupted. */
    if (check_and_set_meta_data(st, lens, exps, cfs, field_char, mon_order,
                elim_block_len, nr_vars, nr_gens, ht_size, nr_threads,
                max_nr_pairs, reset_ht, la_option, reduce_gb, pbm_file,
                info_level)) {
        return 0;
    }

    /* initialize basis */
    bs  = initialize_basis(st->ngens);
    /* initialize basis hash table */
    bht = initialize_basis_hash_table(st);

    import_julia_data(bs, bht, st, lens, exps, cfs);

    if (st->info_level > 0) {
      print_initial_statistics(stderr, st);
    }

    /* for faster divisibility checks, needs to be done after we have
     * read some input data for applying heuristics */
    calculate_divmask(bht);

    /* sort initial elements, smallest lead term first */
    sort_r(bs->hm, (unsigned long)bs->ld, sizeof(hm_t *),
            initial_input_cmp, bht);
    /* normalize input generators */
    if (st->fc > 0) {
        normalize_initial_basis(bs, st->fc);
    } else {
        if (st->fc == 0) {
            remove_content_of_initial_basis(bs);
        }
    }

    *bsp  = bs;
    *bhtp = bht;
    *stp  = st;

    return 1;
}

int core_f4(
        bs_t **bsp,
        ht_t **bhtp,
        stat_t **stp
        )
{
    bs_t *bs    = *bsp;
    ht_t *bht   = *bhtp;
    stat_t *st  = *stp;

    /* timings for one round */
    double rrt0, rrt1;

    /* initialize update hash table, symbolic hash table */
    ht_t *uht = initialize_secondary_hash_table(bht, st);
    ht_t *sht = initialize_secondary_hash_table(bht, st);

    /* hashes-to-columns map, initialized with length 1, is reallocated
     * in each call when generating matrices for linear algebra */
    hi_t *hcm = (hi_t *)malloc(sizeof(hi_t));
    /* matrix holding sparse information generated
     * during symbolic preprocessing */
    mat_t *mat  = (mat_t *)calloc(1, sizeof(mat_t));

    ps_t *ps = initialize_pairset();

    int32_t round, i, j;

    /* reset bs->ld for first update process */
    bs->ld  = 0;

    /* move input generators to basis and generate first spairs.
     * always check redundancy since input generators may be redundant
     * even so they are homogeneous. */
    update_basis(ps, bs, bht, uht, st, st->ngens, 1);

    /* let's start the f4 rounds,  we are done when no more spairs
     * are left in the pairset */
    if (st->info_level > 1) {
        printf("\ndeg     sel   pairs        mat          density \
          new data             time(rd)\n");
        printf("-------------------------------------------------\
----------------------------------------\n");
    }
    for (round = 1; ps->ld > 0; ++round) {
      if (round % st->reset_ht == 0) {
        reset_hash_table(bht, bs, ps, st);
        st->num_rht++;
      }
      rrt0  = realtime();
      st->max_bht_size  = st->max_bht_size > bht->esz ?
        st->max_bht_size : bht->esz;
      st->current_rd  = round;

      /* preprocess data for next reduction round */
      select_spairs_by_minimal_degree(mat, bs, ps, st, sht, bht, NULL);
      symbolic_preprocessing(mat, bs, st, sht, NULL, bht);
      convert_hashes_to_columns(&hcm, mat, st, sht);
      sort_matrix_rows_decreasing(mat->rr, mat->nru);
      sort_matrix_rows_increasing(mat->tr, mat->nrl);
      /* print pbm files of the matrices */
      if (st->gen_pbm_file != 0) {
        write_pbm_file(mat, st);
      }
      /* linear algebra, depending on choice, see set_function_pointers() */
      linear_algebra(mat, bs, st);
      /* columns indices are mapped back to exponent hashes */
      if (mat->np > 0) {
        convert_sparse_matrix_rows_to_basis_elements(
            mat, bs, bht, sht, hcm, st);
      }
      clean_hash_table(sht);
      /* all rows in mat are now polynomials in the basis,
       * so we do not need the rows anymore */
      clear_matrix(mat);

      /* check redundancy only if input is not homogeneous */
      update_basis(ps, bs, bht, uht, st, mat->np, 1-st->homogeneous);

      /* if we found a constant we are done, so remove all remaining pairs */
      if (bs->constant  == 1) {
          ps->ld  = 0;
      }
      rrt1 = realtime();
      if (st->info_level > 1) {
        printf("%13.2f sec\n", rrt1-rrt0);
      }
    }
    if (st->info_level > 1) {
        printf("-------------------------------------------------\
----------------------------------------\n");
    }
    /* remove possible redudant elements */
    j = 0;
    for (i = 0; i < bs->lml; ++i) {
        if (bs->red[bs->lmps[i]] == 0) {
            bs->lm[j]   = bs->lm[i];
            bs->lmps[j] = bs->lmps[i];
            ++j;
        }
    }
    bs->lml = j;

    /* reduce final basis? */
    if (st->reduce_gb == 1) {
        /* note: bht will become sht, and sht will become NULL,
         * thus we need pointers */
        reduce_basis(bs, mat, &hcm, &bht, &sht, st);
    }

    len_t bsctr = 0;
    for (int ii = 0; ii < bs->lml; ++ii) {
        if (bht->ev[bs->hm[bs->lmps[ii]][OFFSET]][0] == 0) {
            bsctr++;
        }
    }
    if (st->nev > 0 && st->info_level > 0) {
        printf("eliminated basis -> %u\n", bsctr);
    }
    *bsp  = bs;
    *bhtp = bht;
    *stp  = st;

    /* free and clean up */
    free(hcm);
    /* note that all rows kept from mat during the overall computation are
     * basis elements and thus we do not need to free the rows itself, but
     * just the matrix structure */
    free(mat);
    if (sht != NULL) {
        free_hash_table(&sht);
    }
    if (uht != NULL) {
        free_hash_table(&uht);
    }
    if (ps != NULL) {
        free_pairset(&ps);
    }

    return 1;
}

int64_t export_results_from_f4(
    /* return values */
    int32_t *bld,   /* basis load */
    int32_t **blen, /* length of each poly in basis */
    int32_t **bexp, /* basis exponent vectors */
    void **bcf,     /* coefficients of basis elements */
    void *(*mallocp) (size_t),
    bs_t **bsp,
    ht_t **bhtp,
    stat_t **stp
    )
{

    bs_t *bs    = *bsp;
    ht_t *bht   = *bhtp;
    stat_t *st  = *stp;

    st->nterms_basis  = export_julia_data(
        bld, blen, bexp, bcf, mallocp, bs, bht, st->fc);
    st->size_basis    = *bld;

    return st->nterms_basis;
}

/* we get from julia the generators as three arrays:
 * 0.  a pointer to an int32_t array for returning the basis to julia
 * 1.  an array of the lengths of each generator
 * 2.  an array of all coefficients of all generators in the order:
 *     first all coefficients of generator 1, then all of generator 2, ...
 * 3.  an array of all exponents of all generators in the order:
 *     first all exponents of generator 1, then all of generator 2, ...
 *
 *  RETURNs the length of the jl_basis array */
int64_t f4_julia(
        void *(*mallocp) (size_t),
        /* return values */
        int32_t *bld,   /* basis load */
        int32_t **blen, /* length of each poly in basis */
        int32_t **bexp, /* basis exponent vectors */
        void **bcf,     /* coefficients of basis elements */
        /* input values */
        const int32_t *lens,
        const int32_t *exps,
        const void *cfs,
        const uint32_t field_char,
        const int32_t mon_order,
        const int32_t elim_block_len,
        const int32_t nr_vars,
        const int32_t nr_gens,
        const int32_t ht_size,
        const int32_t nr_threads,
        const int32_t max_nr_pairs,
        const int32_t reset_ht,
        const int32_t la_option,
        const int32_t reduce_gb,
        const int32_t pbm_file,
        const int32_t info_level
        )
{
    /* timings */
    double ct0, ct1, rt0, rt1;
    ct0 = cputime();
    rt0 = realtime();

    /* data structures for basis, hash table and statistics */
    bs_t *bs    = NULL;
    ht_t *bht   = NULL;
    stat_t *st  = NULL;

    int success = 0;

    success = initialize_f4_input_data(&bs, &bht, &st,
            lens, exps, cfs, field_char, mon_order, elim_block_len,
            nr_vars, nr_gens, ht_size, nr_threads, max_nr_pairs,
            reset_ht, la_option, reduce_gb, pbm_file, info_level);

    if (!success) {
        printf("Bad input data, stopped computation.\n");
        exit(1);
    }

    success = core_f4(&bs, &bht, &st);

    if (!success) {
        printf("Problem with F4, stopped computation.\n");
        exit(1);
    }

    int64_t nterms  = export_results_from_f4(bld, blen, bexp,
            bcf, mallocp, &bs, &bht, &st);

    /* timings */
    ct1 = cputime();
    rt1 = realtime();
    st->overall_ctime = ct1 - ct0;
    st->overall_rtime = rt1 - rt0;

    if (st->info_level > 1) {
      print_final_statistics(stderr, st);
    }

    /* free and clean up */
    free_shared_hash_data(bht);
    if (bht != NULL) {
        free_hash_table(&bht);
    }

    if (bs != NULL) {
        free_basis(&bs);
    }

    free(st);
    st    = NULL;

    return nterms;
}
