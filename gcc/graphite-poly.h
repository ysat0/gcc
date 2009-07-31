/* Graphite polyhedral representation.
   Copyright (C) 2009 Free Software Foundation, Inc.
   Contributed by Sebastian Pop <sebastian.pop@amd.com> and
   Tobias Grosser <grosser@fim.uni-passau.de>.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef GCC_GRAPHITE_POLY_H
#define GCC_GRAPHITE_POLY_H

typedef struct poly_dr *poly_dr_p;
DEF_VEC_P(poly_dr_p);
DEF_VEC_ALLOC_P (poly_dr_p, heap);

typedef struct poly_bb *poly_bb_p;
DEF_VEC_P(poly_bb_p);
DEF_VEC_ALLOC_P (poly_bb_p, heap);

typedef struct scop *scop_p;
DEF_VEC_P(scop_p);
DEF_VEC_ALLOC_P (scop_p, heap);

typedef ppl_dimension_type graphite_dim_t;

static inline graphite_dim_t pbb_dim_iter_domain (const struct poly_bb *);
static inline graphite_dim_t pbb_nb_params (const struct poly_bb *);
static inline graphite_dim_t scop_nb_params (scop_p);

/* A data reference can write or read some memory or we
   just know it may write some memory.  */
enum POLY_DR_TYPE
{
  PDR_READ,
  /* PDR_MAY_READs are represented using PDR_READS. This does not limit the
     expressiveness.  */
  PDR_WRITE,
  PDR_MAY_WRITE
};

struct poly_dr
{
  /* A pointer to compiler's data reference description.  */
  void *compiler_dr;

  /* A pointer to the PBB that contains this data reference.  */
  poly_bb_p pbb;

  enum POLY_DR_TYPE type;

  /* The access polyhedron contains the polyhedral space this data
     reference will access.

     The polyhedron contains these dimensions:

      - The alias set (a):
      Every memory access is classified in at least one alias set.

      - The subscripts (s_0, ..., s_n):
      The memory is accessed using zero or more subscript dimensions.

      - The iteration domain (variables and parameters)

     Do not hardcode the dimensions.  Use the following accessor functions:
     - pdr_alias_set_dim
     - pdr_subscript_dim
     - pdr_iterator_dim
     - pdr_parameter_dim

     Example:

     | int A[1335][123];
     | int *p = malloc ();
     |
     | k = ...
     | for i
     |   {
     |     if (unknown_function ())
     |       p = A;
     |       ... = p[?][?];
     | 	   for j
     |       A[i][j+b] = m;
     |   }

     The data access A[i][j+k] in alias set "5" is described like this:

     | i   j   k   a   s0  s1  1
     | 0   0   0   1   0   0  -5     =  0
     |-1   0   0   0   1   0   0     =  0
     | 0  -1  -1   0   0   1   0     =  0

     The constraints on the data container A[1335][123] are:

     | i   j   k   a   s0  s1  1
     | 0   0   0   0   1   0   0     >= 0
     | 0   0   0   0   0   1   0     >= 0
     | 0   0   0   0  -1   0 1335    >= 0
     | 0   0   0   0   0  -1 123     >= 0

     The pointer "*p" in alias set "5" and "7" is described as a union of
     polyhedron:


     | i   k   a   s0  1
     | 0   0   1   0  -5   =  0
     | 0   0   0   1   0   >= 0

     "or"

     | i   k   a   s0  1
     | 0   0   1   0  -7   =  0
     | 0   0   0   1   0   >= 0

     "*p" accesses all of the object allocated with 'malloc'.

     The scalar data access "m" is represented as an array with zero subscript
     dimensions.

     | i   j   k   a   1
     | 0   0   0  -1   15  = 0 */
  ppl_Pointset_Powerset_C_Polyhedron_t accesses;
  ppl_Pointset_Powerset_C_Polyhedron_t data_container;
};

#define PDR_CDR(PDR) (PDR->compiler_dr)
#define PDR_PBB(PDR) (PDR->pbb)
#define PDR_TYPE(PDR) (PDR->type)
#define PDR_ACCESSES(PDR) (PDR->accesses)
#define PDR_DATA_CONTAINER(PDR) (PDR->data_container)

void new_poly_dr (poly_bb_p, ppl_Pointset_Powerset_C_Polyhedron_t,
		  ppl_Pointset_Powerset_C_Polyhedron_t,
		  enum POLY_DR_TYPE, void *);
void free_poly_dr (poly_dr_p);
void debug_pdr (poly_dr_p);
void print_pdr (FILE *, poly_dr_p);
static inline scop_p pdr_scop (poly_dr_p pdr);

/* The number of subscripts of the PDR.  */

static inline graphite_dim_t
pdr_nb_subscripts (poly_dr_p pdr)
{
  poly_bb_p pbb = PDR_PBB (pdr);
  ppl_dimension_type dim;

  ppl_Pointset_Powerset_C_Polyhedron_space_dimension (PDR_ACCESSES (pdr),
						      &dim);
  return dim - pbb_dim_iter_domain (pbb) - pbb_nb_params (pbb) - 1;
}

/* The dimension of the iteration domain of the scop of PDR.  */

static inline ppl_dimension_type
pdr_dim_iter_domain (poly_dr_p pdr)
{
  return pbb_dim_iter_domain (PDR_PBB (pdr));
}

/* The number of parameters of the scop of PDR.  */

static inline ppl_dimension_type
pdr_nb_params (poly_dr_p pdr)
{
  return scop_nb_params (pdr_scop (pdr));
}

/* The dimension of the accesses polyhedron of PDR.  */

static inline graphite_dim_t
pdr_dim (poly_dr_p pdr)
{
  graphite_dim_t alias_nb_dimensions = 1;

  return pbb_dim_iter_domain (PDR_PBB (pdr)) + alias_nb_dimensions
    + pdr_nb_subscripts (pdr) + scop_nb_params (pdr_scop (pdr));
}

/* The dimension of the alias set in PDR.  */

static inline ppl_dimension_type
pdr_alias_set_dim (poly_dr_p pdr)
{
  poly_bb_p pbb = PDR_PBB (pdr);

  return pbb_dim_iter_domain (pbb) + pbb_nb_params (pbb);
}

/* The dimension in PDR containing subscript S.  */

static inline ppl_dimension_type
pdr_subscript_dim (poly_dr_p pdr, graphite_dim_t s)
{
  poly_bb_p pbb = PDR_PBB (pdr);

  return pbb_dim_iter_domain (pbb) + pbb_nb_params (pbb) + 1 + s;
}

/* The dimension in PDR containing the loop iterator ITER.  */

static inline ppl_dimension_type
pdr_iterator_dim (poly_dr_p pdr ATTRIBUTE_UNUSED, graphite_dim_t iter)
{
  return iter;
}

/* The dimension in PDR containing parameter PARAM.  */

static inline ppl_dimension_type
pdr_parameter_dim (poly_dr_p pdr, graphite_dim_t param)
{
  poly_bb_p pbb = PDR_PBB (pdr);

  return pbb_dim_iter_domain (pbb) + param;
}

/* POLY_BB represents a blackbox in the polyhedral model.  */

struct poly_bb
{
  void *black_box;

  scop_p scop;

  /* The iteration domain of this bb.
     Example:

     for (i = a - 7*b + 8; i <= 3*a + 13*b + 20; i++)
       for (j = 2; j <= 2*i + 5; j++)
         for (k = 0; k <= 5; k++)
           S (i,j,k)

     Loop iterators: i, j, k
     Parameters: a, b

     | i >=  a -  7b +  8
     | i <= 3a + 13b + 20
     | j >= 2
     | j <= 2i + 5
     | k >= 0
     | k <= 5

     The number of variables in the DOMAIN may change and is not
     related to the number of loops in the original code.  */
  ppl_Pointset_Powerset_C_Polyhedron_t domain;

  /* The data references we access.  */
  VEC (poly_dr_p, heap) *drs;

  /* The scattering function containing the transformations.  */
  ppl_Polyhedron_t transformed_scattering;


  /* The original scattering function.  */
  ppl_Polyhedron_t original_scattering;

  /* The number of local variables.  */
  int nb_local_variables;

  /* The number of scattering dimensions in the TRANSFORMED scattering.  */
  int nb_scattering_transform;
};

#define PBB_BLACK_BOX(PBB) ((gimple_bb_p) PBB->black_box)
#define PBB_SCOP(PBB) (PBB->scop)
#define PBB_DOMAIN(PBB) (PBB->domain)
#define PBB_DRS(PBB) (PBB->drs)
#define PBB_TRANSFORMED_SCATTERING(PBB) (PBB->transformed_scattering)
#define PBB_ORIGINAL_SCATTERING(PBB) (PBB->original_scattering)
#define PBB_NB_LOCAL_VARIABLES(PBB) (PBB->nb_local_variables)
#define PBB_NB_SCATTERING_TRANSFORM(PBB) (PBB->nb_scattering_transform)

extern void new_poly_bb (scop_p, void *);
extern void free_poly_bb (poly_bb_p);
extern void debug_loop_vec (poly_bb_p);
extern void schedule_to_scattering (poly_bb_p, int);
extern void print_pbb_domain (FILE *, poly_bb_p);
extern void print_pbb (FILE *, poly_bb_p);
extern void print_scop_context (FILE *, scop_p);
extern void print_scop (FILE *, scop_p);
extern void debug_pbb_domain (poly_bb_p);
extern void debug_pbb (poly_bb_p);
extern void print_pdrs (FILE *, poly_bb_p);
extern void debug_pdrs (poly_bb_p);
extern void debug_scop_context (scop_p);
extern void debug_scop (scop_p);
extern void print_scop_params (FILE *, scop_p);
extern void debug_scop_params (scop_p);
extern void print_iteration_domain (FILE *, poly_bb_p);
extern void print_iteration_domains (FILE *, scop_p);
extern void debug_iteration_domain (poly_bb_p);
extern void debug_iteration_domains (scop_p);
extern bool scop_do_interchange (scop_p);
extern bool scop_do_strip_mine (scop_p);
extern void pbb_number_of_iterations (poly_bb_p, graphite_dim_t, Value);

/* The scop that contains the PDR.  */

static inline scop_p pdr_scop (poly_dr_p pdr)
{
  return PBB_SCOP (PDR_PBB (pdr));
}

/* Set black box of PBB to BLACKBOX.  */

static inline void
pbb_set_black_box (poly_bb_p pbb, void *black_box)
{
  pbb->black_box = black_box;
}

/* The number of loops around PBB: the dimension of the iteration
   domain.  */

static inline graphite_dim_t
pbb_dim_iter_domain (const struct poly_bb *pbb)
{
  scop_p scop = PBB_SCOP (pbb);
  ppl_dimension_type dim;

  ppl_Pointset_Powerset_C_Polyhedron_space_dimension (PBB_DOMAIN (pbb), &dim);
  return dim - scop_nb_params (scop);
}

/* The number of params defined in PBB.  */

static inline graphite_dim_t
pbb_nb_params (const struct poly_bb *pbb)
{
  scop_p scop = PBB_SCOP (pbb);

  return scop_nb_params (scop);
}

/* The number of scattering dimensions in the SCATTERING polyhedron
   of a PBB for a given SCOP.  */

static inline graphite_dim_t
pbb_nb_scattering_orig (const struct poly_bb *pbb)
{
  return 2 * pbb_dim_iter_domain (pbb) + 1;
}

/* The number of scattering dimensions in PBB.  */

static inline graphite_dim_t
pbb_nb_scattering_transform (const struct poly_bb *pbb)
{
  return PBB_NB_SCATTERING_TRANSFORM (pbb);
}

/* Returns the number of local variables used in the transformed
   scattering polyhedron of PBB.  */

static inline graphite_dim_t
pbb_nb_local_vars (const struct poly_bb *pbb)
{
  /* For now we do not have any local variables, as we do not do strip
     mining for example.  */
  return PBB_NB_LOCAL_VARIABLES (pbb);
}

/* The dimension in the domain of PBB containing the iterator ITER.  */

static inline ppl_dimension_type
pbb_iterator_dim (poly_bb_p pbb ATTRIBUTE_UNUSED, graphite_dim_t iter)
{
  return iter;
}

/* The dimension in the domain of PBB containing the iterator ITER.  */

static inline ppl_dimension_type
pbb_parameter_dim (poly_bb_p pbb, graphite_dim_t param)
{
  return param
    + pbb_dim_iter_domain (pbb);
}

/* The dimension in the original scattering polyhedron of PBB
   containing the scattering iterator SCATTER.  */

static inline ppl_dimension_type
psco_scattering_dim (poly_bb_p pbb ATTRIBUTE_UNUSED, graphite_dim_t scatter)
{
  gcc_assert (scatter < pbb_nb_scattering_orig (pbb));
  return scatter;
}

/* The dimension in the transformed scattering polyhedron of PBB
   containing the scattering iterator SCATTER.  */

static inline ppl_dimension_type
psct_scattering_dim (poly_bb_p pbb ATTRIBUTE_UNUSED, graphite_dim_t scatter)
{
  gcc_assert (scatter <= pbb_nb_scattering_transform (pbb));
  return scatter;
}

ppl_dimension_type psct_scattering_dim_for_loop_depth (poly_bb_p,
						       graphite_dim_t);

/* The dimension in the transformed scattering polyhedron of PBB of
   the local variable LV.  */

static inline ppl_dimension_type
psct_local_var_dim (poly_bb_p pbb, graphite_dim_t lv)
{
  gcc_assert (lv <= pbb_nb_local_vars (pbb));
  return lv + pbb_nb_scattering_transform (pbb);
}

/* The dimension in the original scattering polyhedron of PBB
   containing the loop iterator ITER.  */

static inline ppl_dimension_type
psco_iterator_dim (poly_bb_p pbb, graphite_dim_t iter)
{
  gcc_assert (iter < pbb_dim_iter_domain (pbb));
  return iter + pbb_nb_scattering_orig (pbb);
}

/* The dimension in the transformed scattering polyhedron of PBB
   containing the loop iterator ITER.  */

static inline ppl_dimension_type
psct_iterator_dim (poly_bb_p pbb, graphite_dim_t iter)
{
  gcc_assert (iter < pbb_dim_iter_domain (pbb));
  return iter
    + pbb_nb_scattering_transform (pbb)
    + pbb_nb_local_vars (pbb);
}

/* The dimension in the original scattering polyhedron of PBB
   containing parameter PARAM.  */

static inline ppl_dimension_type
psco_parameter_dim (poly_bb_p pbb, graphite_dim_t param)
{
  gcc_assert (param < pbb_nb_params (pbb));
  return param
    + pbb_nb_scattering_orig (pbb)
    + pbb_dim_iter_domain (pbb);
}

/* The dimension in the transformed scattering polyhedron of PBB
   containing parameter PARAM.  */

static inline ppl_dimension_type
psct_parameter_dim (poly_bb_p pbb, graphite_dim_t param)
{
  gcc_assert (param < pbb_nb_params (pbb));
  return param
    + pbb_nb_scattering_transform (pbb)
    + pbb_nb_local_vars (pbb)
    + pbb_dim_iter_domain (pbb);
}

/* Adds to the transformed scattering polyhedron of PBB a new local
   variable and returns its index.  */

static inline graphite_dim_t
psct_add_local_variable (poly_bb_p pbb)
{
  graphite_dim_t nlv = pbb_nb_local_vars (pbb);
  ppl_dimension_type lv_column = psct_local_var_dim (pbb, nlv);
  ppl_insert_dimensions (PBB_TRANSFORMED_SCATTERING (pbb), lv_column, 1);
  PBB_NB_LOCAL_VARIABLES (pbb) += 1;
  return nlv;
}

/* Adds a dimension to the transformed scattering polyhedron of PBB at
   INDEX.  */

static inline void
psct_add_scattering_dimension (poly_bb_p pbb, ppl_dimension_type index)
{
  gcc_assert (index < pbb_nb_scattering_transform (pbb));

  ppl_insert_dimensions (PBB_TRANSFORMED_SCATTERING (pbb), index, 1);
  PBB_NB_SCATTERING_TRANSFORM (pbb) += 1;
}

/* A SCOP is a Static Control Part of the program, simple enough to be
   represented in polyhedral form.  */
struct scop
{
  /* A SCOP is defined as a SESE region.  */
  void *region;

  /* Number of parameters in SCoP.  */
  graphite_dim_t nb_params;

  /* All the basic blocks in this scop that contain memory references
     and that will be represented as statements in the polyhedral
     representation.  */
  VEC (poly_bb_p, heap) *bbs;

  /* Data dependence graph for this SCoP.  */
  struct graph *dep_graph;

  /* The context describes known restrictions concerning the parameters
     and relations in between the parameters.

  void f (int8_t a, uint_16_t b) {
    c = 2 a + b;
    ...
  }

  Here we can add these restrictions to the context:

  -128 >= a >= 127
     0 >= b >= 65,535
     c = 2a + b  */
  ppl_Pointset_Powerset_C_Polyhedron_t context;

  /* A hashtable of the original pairs of dependent data references.
     For each pair of dependent data references, the dependence
     polyhedron is stored also.  */
  htab_t original_pdr_pairs;
};

#define SCOP_BBS(S) (S->bbs)
#define SCOP_REGION(S) ((sese) S->region)
#define SCOP_DEP_GRAPH(S) (S->dep_graph)
#define SCOP_CONTEXT(S) (S->context)
#define SCOP_ORIGINAL_PDR_PAIRS(S) (S->original_pdr_pairs)

extern scop_p new_scop (void *);
extern void free_scop (scop_p);
extern void free_scops (VEC (scop_p, heap) *);
extern void print_generated_program (FILE *, scop_p);
extern void debug_generated_program (scop_p);
extern void print_scattering_function (FILE *, poly_bb_p);
extern void print_scattering_functions (FILE *, scop_p);
extern void debug_scattering_function (poly_bb_p);
extern void debug_scattering_functions (scop_p);
extern int scop_max_loop_depth (scop_p);
extern int unify_scattering_dimensions (scop_p);
extern bool apply_poly_transforms (scop_p);
extern bool graphite_legal_transform (scop_p);

/* Set the region of SCOP to REGION.  */

static inline void
scop_set_region (scop_p scop, void *region)
{
  scop->region = region;
}

/* Returns the number of parameters for SCOP.  */

static inline graphite_dim_t
scop_nb_params (scop_p scop)
{
  return scop->nb_params;
}

/* Set the number of params of SCOP to NB_PARAMS.  */

static inline void
scop_set_nb_params (scop_p scop, graphite_dim_t nb_params)
{
  scop->nb_params = nb_params;
}

#endif
