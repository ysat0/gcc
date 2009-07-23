;; Expander definitions for vector support.  No instructions are in this file,
;; this file provides the generic vector expander, and the actual vector
;; instructions will be in altivec.md.

;; Copyright (C) 2009
;; Free Software Foundation, Inc.
;; Contributed by Michael Meissner <meissner@linux.vnet.ibm.com>

;; This file is part of GCC.

;; GCC is free software; you can redistribute it and/or modify it
;; under the terms of the GNU General Public License as published
;; by the Free Software Foundation; either version 3, or (at your
;; option) any later version.

;; GCC is distributed in the hope that it will be useful, but WITHOUT
;; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
;; or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
;; License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GCC; see the file COPYING3.  If not see
;; <http://www.gnu.org/licenses/>.


;; Vector int modes
(define_mode_iterator VEC_I [V16QI V8HI V4SI])

;; Vector float modes
(define_mode_iterator VEC_F [V4SF])

;; Vector arithmetic modes
(define_mode_iterator VEC_A [V16QI V8HI V4SI V4SF])

;; Vector modes that need alginment via permutes
(define_mode_iterator VEC_K [V16QI V8HI V4SI V4SF])

;; Vector logical modes
(define_mode_iterator VEC_L [V16QI V8HI V4SI V2DI V4SF V2DF TI])

;; Vector modes for moves.  Don't do TImode here.
(define_mode_iterator VEC_M [V16QI V8HI V4SI V2DI V4SF V2DF])

;; Vector comparison modes
(define_mode_iterator VEC_C [V16QI V8HI V4SI V4SF V2DF])

;; Vector init/extract modes
(define_mode_iterator VEC_E [V16QI V8HI V4SI V2DI V4SF V2DF])

;; Vector reload iterator
(define_mode_iterator VEC_R [V16QI V8HI V4SI V2DI V4SF V2DF DF TI])

;; Base type from vector mode
(define_mode_attr VEC_base [(V16QI "QI")
			    (V8HI  "HI")
			    (V4SI  "SI")
			    (V2DI  "DI")
			    (V4SF  "SF")
			    (V2DF  "DF")
			    (TI    "TI")])

;; Same size integer type for floating point data
(define_mode_attr VEC_int [(V4SF  "v4si")
			   (V2DF  "v2di")])

(define_mode_attr VEC_INT [(V4SF  "V4SI")
			   (V2DF  "V2DI")])

;; constants for unspec
(define_constants
  [(UNSPEC_PREDICATE	400)])


;; Vector move instructions.
(define_expand "mov<mode>"
  [(set (match_operand:VEC_M 0 "nonimmediate_operand" "")
	(match_operand:VEC_M 1 "any_operand" ""))]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
{
  if (can_create_pseudo_p ())
    {
      if (CONSTANT_P (operands[1])
	  && !easy_vector_constant (operands[1], <MODE>mode))
	operands[1] = force_const_mem (<MODE>mode, operands[1]);

      else if (!vlogical_operand (operands[0], <MODE>mode)
	       && !vlogical_operand (operands[1], <MODE>mode))
	operands[1] = force_reg (<MODE>mode, operands[1]);
    }
})

;; Generic vector floating point load/store instructions.
(define_expand "vector_load_<mode>"
  [(set (match_operand:VEC_M 0 "vfloat_operand" "")
	(match_operand:VEC_M 1 "memory_operand" ""))]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "vector_store_<mode>"
  [(set (match_operand:VEC_M 0 "memory_operand" "")
	(match_operand:VEC_M 1 "vfloat_operand" ""))]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
  "")

;; Splits if a GPR register was chosen for the move
(define_split
  [(set (match_operand:VEC_L 0 "nonimmediate_operand" "")
        (match_operand:VEC_L 1 "input_operand" ""))]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)
   && reload_completed
   && gpr_or_gpr_p (operands[0], operands[1])"
  [(pc)]
{
  rs6000_split_multireg_move (operands[0], operands[1]);
  DONE;
})


;; Reload patterns for vector operations.  We may need an addtional base
;; register to convert the reg+offset addressing to reg+reg for vector
;; registers and reg+reg or (reg+reg)&(-16) addressing to just an index
;; register for gpr registers.
(define_expand "reload_<VEC_R:mode>_<P:mptrsize>_store"
  [(parallel [(match_operand:VEC_R 0 "memory_operand" "m")
              (match_operand:VEC_R 1 "gpc_reg_operand" "r")
              (match_operand:P 2 "register_operand" "=&b")])]
  "<P:tptrsize>"
{
  rs6000_secondary_reload_inner (operands[1], operands[0], operands[2], true);
  DONE;
})

(define_expand "reload_<VEC_R:mode>_<P:mptrsize>_load"
  [(parallel [(match_operand:VEC_R 0 "gpc_reg_operand" "=&r")
              (match_operand:VEC_R 1 "memory_operand" "m")
              (match_operand:P 2 "register_operand" "=&b")])]
  "<P:tptrsize>"
{
  rs6000_secondary_reload_inner (operands[0], operands[1], operands[2], false);
  DONE;
})

;; Reload sometimes tries to move the address to a GPR, and can generate
;; invalid RTL for addresses involving AND -16.  Allow addresses involving
;; reg+reg, reg+small constant, or just reg, all wrapped in an AND -16.

(define_insn_and_split "*vec_reload_and_plus_<mptrsize>"
  [(set (match_operand:P 0 "gpc_reg_operand" "=b")
	(and:P (plus:P (match_operand:P 1 "gpc_reg_operand" "r")
		       (match_operand:P 2 "reg_or_cint_operand" "rI"))
	       (const_int -16)))]
  "TARGET_ALTIVEC && (reload_in_progress || reload_completed)"
  "#"
  "&& reload_completed"
  [(set (match_dup 0)
	(plus:P (match_dup 1)
		(match_dup 2)))
   (parallel [(set (match_dup 0)
		   (and:P (match_dup 0)
			  (const_int -16)))
	      (clobber:CC (scratch:CC))])])

;; The normal ANDSI3/ANDDI3 won't match if reload decides to move an AND -16
;; address to a register because there is no clobber of a (scratch), so we add
;; it here.
(define_insn_and_split "*vec_reload_and_reg_<mptrsize>"
  [(set (match_operand:P 0 "gpc_reg_operand" "=b")
	(and:P (match_operand:P 1 "gpc_reg_operand" "r")
	       (const_int -16)))]
  "TARGET_ALTIVEC && (reload_in_progress || reload_completed)"
  "#"
  "&& reload_completed"
  [(parallel [(set (match_dup 0)
		   (and:P (match_dup 1)
			  (const_int -16)))
	      (clobber:CC (scratch:CC))])])

;; Generic floating point vector arithmetic support
(define_expand "add<mode>3"
  [(set (match_operand:VEC_F 0 "vfloat_operand" "")
	(plus:VEC_F (match_operand:VEC_F 1 "vfloat_operand" "")
		    (match_operand:VEC_F 2 "vfloat_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "sub<mode>3"
  [(set (match_operand:VEC_F 0 "vfloat_operand" "")
	(minus:VEC_F (match_operand:VEC_F 1 "vfloat_operand" "")
		     (match_operand:VEC_F 2 "vfloat_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "mul<mode>3"
  [(set (match_operand:VEC_F 0 "vfloat_operand" "")
	(mult:VEC_F (match_operand:VEC_F 1 "vfloat_operand" "")
		    (match_operand:VEC_F 2 "vfloat_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode) && TARGET_FUSED_MADD"
  "
{
  emit_insn (gen_altivec_mulv4sf3 (operands[0], operands[1], operands[2]));
  DONE;
}")

(define_expand "neg<mode>2"
  [(set (match_operand:VEC_F 0 "vfloat_operand" "")
	(neg:VEC_F (match_operand:VEC_F 1 "vfloat_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "
{
  emit_insn (gen_altivec_negv4sf2 (operands[0], operands[1]));
  DONE;
}")

(define_expand "abs<mode>2"
  [(set (match_operand:VEC_F 0 "vfloat_operand" "")
	(abs:VEC_F (match_operand:VEC_F 1 "vfloat_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "
{
  emit_insn (gen_altivec_absv4sf2 (operands[0], operands[1]));
  DONE;
}")

(define_expand "smin<mode>3"
  [(set (match_operand:VEC_F 0 "register_operand" "")
        (smin:VEC_F (match_operand:VEC_F 1 "register_operand" "")
		    (match_operand:VEC_F 2 "register_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "smax<mode>3"
  [(set (match_operand:VEC_F 0 "register_operand" "")
        (smax:VEC_F (match_operand:VEC_F 1 "register_operand" "")
		    (match_operand:VEC_F 2 "register_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")


(define_expand "ftrunc<mode>2"
  [(set (match_operand:VEC_F 0 "vfloat_operand" "")
  	(fix:VEC_F (match_operand:VEC_F 1 "vfloat_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")


;; Vector comparisons
(define_expand "vcond<mode>"
  [(set (match_operand:VEC_F 0 "vfloat_operand" "")
	(if_then_else:VEC_F
	 (match_operator 3 "comparison_operator"
			 [(match_operand:VEC_F 4 "vfloat_operand" "")
			  (match_operand:VEC_F 5 "vfloat_operand" "")])
	 (match_operand:VEC_F 1 "vfloat_operand" "")
	 (match_operand:VEC_F 2 "vfloat_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "
{
  if (rs6000_emit_vector_cond_expr (operands[0], operands[1], operands[2],
				    operands[3], operands[4], operands[5]))
    DONE;
  else
    FAIL;
}")

(define_expand "vcond<mode>"
  [(set (match_operand:VEC_I 0 "vint_operand" "")
	(if_then_else:VEC_I
	 (match_operator 3 "comparison_operator"
			 [(match_operand:VEC_I 4 "vint_operand" "")
			  (match_operand:VEC_I 5 "vint_operand" "")])
	 (match_operand:VEC_I 1 "vint_operand" "")
	 (match_operand:VEC_I 2 "vint_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "
{
  if (rs6000_emit_vector_cond_expr (operands[0], operands[1], operands[2],
				    operands[3], operands[4], operands[5]))
    DONE;
  else
    FAIL;
}")

(define_expand "vcondu<mode>"
  [(set (match_operand:VEC_I 0 "vint_operand" "")
	(if_then_else:VEC_I
	 (match_operator 3 "comparison_operator"
			 [(match_operand:VEC_I 4 "vint_operand" "")
			  (match_operand:VEC_I 5 "vint_operand" "")])
	 (match_operand:VEC_I 1 "vint_operand" "")
	 (match_operand:VEC_I 2 "vint_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "
{
  if (rs6000_emit_vector_cond_expr (operands[0], operands[1], operands[2],
				    operands[3], operands[4], operands[5]))
    DONE;
  else
    FAIL;
}")

(define_expand "vector_eq<mode>"
  [(set (match_operand:VEC_C 0 "vlogical_operand" "")
	(eq:VEC_C (match_operand:VEC_C 1 "vlogical_operand" "")
		  (match_operand:VEC_C 2 "vlogical_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "vector_gt<mode>"
  [(set (match_operand:VEC_C 0 "vlogical_operand" "")
	(gt:VEC_C (match_operand:VEC_C 1 "vlogical_operand" "")
		  (match_operand:VEC_C 2 "vlogical_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "vector_ge<mode>"
  [(set (match_operand:VEC_C 0 "vlogical_operand" "")
	(ge:VEC_C (match_operand:VEC_C 1 "vlogical_operand" "")
		  (match_operand:VEC_C 2 "vlogical_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "vector_gtu<mode>"
  [(set (match_operand:VEC_I 0 "vint_operand" "")
	(gtu:VEC_I (match_operand:VEC_I 1 "vint_operand" "")
		   (match_operand:VEC_I 2 "vint_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "vector_geu<mode>"
  [(set (match_operand:VEC_I 0 "vint_operand" "")
	(geu:VEC_I (match_operand:VEC_I 1 "vint_operand" "")
		   (match_operand:VEC_I 2 "vint_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

;; Note the arguments for __builtin_altivec_vsel are op2, op1, mask
;; which is in the reverse order that we want
(define_expand "vector_select_<mode>"
  [(set (match_operand:VEC_L 0 "vlogical_operand" "")
	(if_then_else:VEC_L
	 (ne:CC (match_operand:VEC_L 3 "vlogical_operand" "")
		(const_int 0))
	 (match_operand:VEC_L 2 "vlogical_operand" "")
	 (match_operand:VEC_L 1 "vlogical_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "vector_select_<mode>_uns"
  [(set (match_operand:VEC_L 0 "vlogical_operand" "")
	(if_then_else:VEC_L
	 (ne:CCUNS (match_operand:VEC_L 3 "vlogical_operand" "")
		   (const_int 0))
	 (match_operand:VEC_L 2 "vlogical_operand" "")
	 (match_operand:VEC_L 1 "vlogical_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

;; Expansions that compare vectors producing a vector result and a predicate,
;; setting CR6 to indicate a combined status
(define_expand "vector_eq_<mode>_p"
  [(parallel
    [(set (reg:CC 74)
	  (unspec:CC [(eq:CC (match_operand:VEC_A 1 "vlogical_operand" "")
			     (match_operand:VEC_A 2 "vlogical_operand" ""))]
		     UNSPEC_PREDICATE))
     (set (match_operand:VEC_A 0 "vlogical_operand" "")
	  (eq:VEC_A (match_dup 1)
		    (match_dup 2)))])]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "vector_gt_<mode>_p"
  [(parallel
    [(set (reg:CC 74)
	  (unspec:CC [(gt:CC (match_operand:VEC_A 1 "vlogical_operand" "")
			     (match_operand:VEC_A 2 "vlogical_operand" ""))]
		     UNSPEC_PREDICATE))
     (set (match_operand:VEC_A 0 "vlogical_operand" "")
	  (gt:VEC_A (match_dup 1)
		    (match_dup 2)))])]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "vector_ge_<mode>_p"
  [(parallel
    [(set (reg:CC 74)
	  (unspec:CC [(ge:CC (match_operand:VEC_F 1 "vfloat_operand" "")
			     (match_operand:VEC_F 2 "vfloat_operand" ""))]
		     UNSPEC_PREDICATE))
     (set (match_operand:VEC_F 0 "vfloat_operand" "")
	  (ge:VEC_F (match_dup 1)
		    (match_dup 2)))])]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "vector_gtu_<mode>_p"
  [(parallel
    [(set (reg:CC 74)
	  (unspec:CC [(gtu:CC (match_operand:VEC_I 1 "vint_operand" "")
			      (match_operand:VEC_I 2 "vint_operand" ""))]
		     UNSPEC_PREDICATE))
     (set (match_operand:VEC_I 0 "vlogical_operand" "")
	  (gtu:VEC_I (match_dup 1)
		     (match_dup 2)))])]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "")

;; AltiVec predicates.

(define_expand "cr6_test_for_zero"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(eq:SI (reg:CC 74)
	       (const_int 0)))]
  "TARGET_ALTIVEC"
  "")	

(define_expand "cr6_test_for_zero_reverse"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(eq:SI (reg:CC 74)
	       (const_int 0)))
   (set (match_dup 0) (minus:SI (const_int 1) (match_dup 0)))]
  "TARGET_ALTIVEC"
  "")

(define_expand "cr6_test_for_lt"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(lt:SI (reg:CC 74)
	       (const_int 0)))]
  "TARGET_ALTIVEC"
  "")

(define_expand "cr6_test_for_lt_reverse"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(lt:SI (reg:CC 74)
	       (const_int 0)))
   (set (match_dup 0) (minus:SI (const_int 1) (match_dup 0)))]
  "TARGET_ALTIVEC"
  "")


;; Vector logical instructions
(define_expand "xor<mode>3"
  [(set (match_operand:VEC_L 0 "vlogical_operand" "")
        (xor:VEC_L (match_operand:VEC_L 1 "vlogical_operand" "")
		   (match_operand:VEC_L 2 "vlogical_operand" "")))]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "ior<mode>3"
  [(set (match_operand:VEC_L 0 "vlogical_operand" "")
        (ior:VEC_L (match_operand:VEC_L 1 "vlogical_operand" "")
		   (match_operand:VEC_L 2 "vlogical_operand" "")))]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "and<mode>3"
  [(set (match_operand:VEC_L 0 "vlogical_operand" "")
        (and:VEC_L (match_operand:VEC_L 1 "vlogical_operand" "")
		   (match_operand:VEC_L 2 "vlogical_operand" "")))]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "one_cmpl<mode>2"
  [(set (match_operand:VEC_L 0 "vlogical_operand" "")
        (not:VEC_L (match_operand:VEC_L 1 "vlogical_operand" "")))]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
  "")
  
(define_expand "nor<mode>3"
  [(set (match_operand:VEC_L 0 "vlogical_operand" "")
        (not:VEC_L (ior:VEC_L (match_operand:VEC_L 1 "vlogical_operand" "")
			      (match_operand:VEC_L 2 "vlogical_operand" ""))))]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
  "")

(define_expand "andc<mode>3"
  [(set (match_operand:VEC_L 0 "vlogical_operand" "")
        (and:VEC_L (not:VEC_L (match_operand:VEC_L 2 "vlogical_operand" ""))
		   (match_operand:VEC_L 1 "vlogical_operand" "")))]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
  "")

;; Same size conversions
(define_expand "float<VEC_int><mode>2"
  [(set (match_operand:VEC_F 0 "vfloat_operand" "")
	(float:VEC_F (match_operand:<VEC_INT> 1 "vint_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "
{
  emit_insn (gen_altivec_vcfsx (operands[0], operands[1], const0_rtx));
  DONE;
}")

(define_expand "unsigned_float<VEC_int><mode>2"
  [(set (match_operand:VEC_F 0 "vfloat_operand" "")
	(unsigned_float:VEC_F (match_operand:<VEC_INT> 1 "vint_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "
{
  emit_insn (gen_altivec_vcfux (operands[0], operands[1], const0_rtx));
  DONE;
}")

(define_expand "fix_trunc<mode><VEC_int>2"
  [(set (match_operand:<VEC_INT> 0 "vint_operand" "")
	(fix:<VEC_INT> (match_operand:VEC_F 1 "vfloat_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "
{
  emit_insn (gen_altivec_vctsxs (operands[0], operands[1], const0_rtx));
  DONE;
}")

(define_expand "fixuns_trunc<mode><VEC_int>2"
  [(set (match_operand:<VEC_INT> 0 "vint_operand" "")
	(unsigned_fix:<VEC_INT> (match_operand:VEC_F 1 "vfloat_operand" "")))]
  "VECTOR_UNIT_ALTIVEC_P (<MODE>mode)"
  "
{
  emit_insn (gen_altivec_vctuxs (operands[0], operands[1], const0_rtx));
  DONE;
}")


;; Vector initialization, set, extract
(define_expand "vec_init<mode>"
  [(match_operand:VEC_E 0 "vlogical_operand" "")
   (match_operand:VEC_E 1 "" "")]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
{
  rs6000_expand_vector_init (operands[0], operands[1]);
  DONE;
})

(define_expand "vec_set<mode>"
  [(match_operand:VEC_E 0 "vlogical_operand" "")
   (match_operand:<VEC_base> 1 "register_operand" "")
   (match_operand 2 "const_int_operand" "")]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
{
  rs6000_expand_vector_set (operands[0], operands[1], INTVAL (operands[2]));
  DONE;
})

(define_expand "vec_extract<mode>"
  [(match_operand:<VEC_base> 0 "register_operand" "")
   (match_operand:VEC_E 1 "vlogical_operand" "")
   (match_operand 2 "const_int_operand" "")]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
{
  rs6000_expand_vector_extract (operands[0], operands[1],
				INTVAL (operands[2]));
  DONE;
})

;; Interleave patterns
(define_expand "vec_interleave_highv4sf"
  [(set (match_operand:V4SF 0 "vfloat_operand" "")
        (vec_merge:V4SF
	 (vec_select:V4SF (match_operand:V4SF 1 "vfloat_operand" "")
			  (parallel [(const_int 0)
				     (const_int 2)
				     (const_int 1)
				     (const_int 3)]))
	 (vec_select:V4SF (match_operand:V4SF 2 "vfloat_operand" "")
			  (parallel [(const_int 2)
				     (const_int 0)
				     (const_int 3)
				     (const_int 1)]))
	 (const_int 5)))]
  "VECTOR_UNIT_ALTIVEC_P (V4SFmode)"
  "")

(define_expand "vec_interleave_lowv4sf"
  [(set (match_operand:V4SF 0 "vfloat_operand" "")
        (vec_merge:V4SF
	 (vec_select:V4SF (match_operand:V4SF 1 "vfloat_operand" "")
			  (parallel [(const_int 2)
				     (const_int 0)
				     (const_int 3)
				     (const_int 1)]))
	 (vec_select:V4SF (match_operand:V4SF 2 "vfloat_operand" "")
			  (parallel [(const_int 0)
				     (const_int 2)
				     (const_int 1)
				     (const_int 3)]))
	 (const_int 5)))]
  "VECTOR_UNIT_ALTIVEC_P (V4SFmode)"
  "")


;; Align vector loads with a permute.
(define_expand "vec_realign_load_<mode>"
  [(match_operand:VEC_K 0 "vlogical_operand" "")
   (match_operand:VEC_K 1 "vlogical_operand" "")
   (match_operand:VEC_K 2 "vlogical_operand" "")
   (match_operand:V16QI 3 "vlogical_operand" "")]
  "VECTOR_MEM_ALTIVEC_P (<MODE>mode)"
{
  emit_insn (gen_altivec_vperm_<mode> (operands[0], operands[1], operands[2],
				       operands[3]));
  DONE;
})


;; Vector shift left in bits.  Currently supported ony for shift
;; amounts that can be expressed as byte shifts (divisible by 8).
;; General shift amounts can be supported using vslo + vsl. We're
;; not expecting to see these yet (the vectorizer currently
;; generates only shifts divisible by byte_size).
(define_expand "vec_shl_<mode>"
  [(match_operand:VEC_L 0 "vlogical_operand" "")
   (match_operand:VEC_L 1 "vlogical_operand" "")
   (match_operand:QI 2 "reg_or_short_operand" "")]
  "TARGET_ALTIVEC"
  "
{
  rtx bitshift = operands[2];
  rtx shift;
  rtx insn;
  HOST_WIDE_INT bitshift_val;
  HOST_WIDE_INT byteshift_val;

  if (! CONSTANT_P (bitshift))
    FAIL;
  bitshift_val = INTVAL (bitshift);
  if (bitshift_val & 0x7)
    FAIL;
  byteshift_val = bitshift_val >> 3;
  shift = gen_rtx_CONST_INT (QImode, byteshift_val);
  insn = gen_altivec_vsldoi_<mode> (operands[0], operands[1], operands[1],
				    shift);

  emit_insn (insn);
  DONE;
}")

;; Vector shift right in bits. Currently supported ony for shift
;; amounts that can be expressed as byte shifts (divisible by 8).
;; General shift amounts can be supported using vsro + vsr. We're
;; not expecting to see these yet (the vectorizer currently
;; generates only shifts divisible by byte_size).
(define_expand "vec_shr_<mode>"
  [(match_operand:VEC_L 0 "vlogical_operand" "")
   (match_operand:VEC_L 1 "vlogical_operand" "")
   (match_operand:QI 2 "reg_or_short_operand" "")]
  "TARGET_ALTIVEC"
  "
{
  rtx bitshift = operands[2];
  rtx shift;
  rtx insn;
  HOST_WIDE_INT bitshift_val;
  HOST_WIDE_INT byteshift_val;
 
  if (! CONSTANT_P (bitshift))
    FAIL;
  bitshift_val = INTVAL (bitshift);
  if (bitshift_val & 0x7)
    FAIL;
  byteshift_val = 16 - (bitshift_val >> 3);
  shift = gen_rtx_CONST_INT (QImode, byteshift_val);
  insn = gen_altivec_vsldoi_<mode> (operands[0], operands[1], operands[1],
				    shift);

  emit_insn (insn);
  DONE;
}")

;; Expanders for rotate each element in a vector
(define_expand "vrotl<mode>3"
  [(set (match_operand:VEC_I 0 "vint_operand" "")
	(rotate:VEC_I (match_operand:VEC_I 1 "vint_operand" "")
		      (match_operand:VEC_I 2 "vint_operand" "")))]
  "TARGET_ALTIVEC"
  "")

;; Expanders for arithmetic shift left on each vector element
(define_expand "vashl<mode>3"
  [(set (match_operand:VEC_I 0 "vint_operand" "")
	(ashift:VEC_I (match_operand:VEC_I 1 "vint_operand" "")
		      (match_operand:VEC_I 2 "vint_operand" "")))]
  "TARGET_ALTIVEC"
  "")

;; Expanders for logical shift right on each vector element
(define_expand "vlshr<mode>3"
  [(set (match_operand:VEC_I 0 "vint_operand" "")
	(lshiftrt:VEC_I (match_operand:VEC_I 1 "vint_operand" "")
			(match_operand:VEC_I 2 "vint_operand" "")))]
  "TARGET_ALTIVEC"
  "")

;; Expanders for arithmetic shift right on each vector element
(define_expand "vashr<mode>3"
  [(set (match_operand:VEC_I 0 "vint_operand" "")
	(ashiftrt:VEC_I (match_operand:VEC_I 1 "vint_operand" "")
			(match_operand:VEC_I 2 "vint_operand" "")))]
  "TARGET_ALTIVEC"
  "")
