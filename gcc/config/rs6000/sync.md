;; Machine description for PowerPC synchronization instructions.
;; Copyright (C) 2005 Free Software Foundation, Inc.
;; Contributed by Geoffrey Keating.

;; This file is part of GCC.

;; GCC is free software; you can redistribute it and/or modify it
;; under the terms of the GNU General Public License as published
;; by the Free Software Foundation; either version 2, or (at your
;; option) any later version.

;; GCC is distributed in the hope that it will be useful, but WITHOUT
;; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
;; or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
;; License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GCC; see the file COPYING.  If not, write to the
;; Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
;; MA 02110-1301, USA.

(define_mode_attr larx [(SI "lwarx") (DI "ldarx")])
(define_mode_attr stcx [(SI "stwcx.") (DI "stdcx.")])

(define_insn "memory_barrier"
  [(set (mem:BLK (match_scratch 0 "X"))
	(unspec:BLK [(mem:BLK (match_scratch 1 "X"))] UNSPEC_SYNC))]
  ""
  "{ics|sync}")

(define_expand "sync_compare_and_swap<mode>"
  [(parallel [(set (match_operand:GPR 1 "memory_operand" "")
		   (unspec:GPR [(match_dup 1)
				(match_operand:GPR 2 "reg_or_short_operand" "")
				(match_operand:GPR 3 "gpc_reg_operand" "")]
			       UNSPEC_SYNC_SWAP))
	      (set (match_operand:GPR 0 "gpc_reg_operand" "") (match_dup 1))
	      (set (mem:BLK (match_scratch 5 ""))
		   (unspec:BLK [(mem:BLK (match_scratch 6 ""))] UNSPEC_SYNC))
	      (clobber (match_scratch:CC 4 ""))])]
  "TARGET_POWERPC")

(define_insn "sync_compare_and_swap<mode>_internal"
  [(set (match_operand:GPR 1 "memory_operand" "+Z")
	(unspec:GPR [(match_dup 1)
		     (match_operand:GPR 2 "reg_or_short_operand" "rI")
		     (match_operand:GPR 3 "gpc_reg_operand" "r")]
		    UNSPEC_SYNC_SWAP))
   (set (match_operand:GPR 0 "gpc_reg_operand" "=&r") (match_dup 1))
   (set (mem:BLK (match_scratch 5 "X"))
	(unspec:BLK [(mem:BLK (match_scratch 6 "X"))] UNSPEC_SYNC))
   (clobber (match_scratch:CC 4 "=&x"))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "sync\n\t<larx> %0,%y1\n\tcmp<wd>%I2 %0,%2\n\tbne- $+12\n\t<stcx> %3,%y1\n\tbne- $-16\n\tisync"
  [(set_attr "length" "28")])

(define_insn "sync_compare_and_swap<mode>_ppc405"
  [(set (match_operand:GPR 1 "memory_operand" "+Z")
	(unspec:GPR [(match_dup 1)
		     (match_operand:GPR 2 "reg_or_short_operand" "rI")
		     (match_operand:GPR 3 "gpc_reg_operand" "r")]
		    UNSPEC_SYNC_SWAP))
   (set (match_operand:GPR 0 "gpc_reg_operand" "=&r") (match_dup 1))
   (set (mem:BLK (match_scratch 5 "X"))
	(unspec:BLK [(mem:BLK (match_scratch 6 "X"))] UNSPEC_SYNC))
   (clobber (match_scratch:CC 4 "=&x"))]
  "TARGET_POWERPC && PPC405_ERRATUM77"
  "sync\n\t<larx> %0,%y1\n\tcmp<wd>%I2 %0,%2\n\tbne- $+12\n\tsync\n\t<stcx> %3,%y1\n\tbne- $-16\n\tisync"
  [(set_attr "length" "32")])

(define_expand "sync_add<mode>"
  [(use (match_operand:INT1 0 "memory_operand" ""))
   (use (match_operand:INT1 1 "add_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (PLUS, <MODE>mode, operands[0], operands[1], 
		    NULL_RTX, NULL_RTX, true);
  DONE;
}")

(define_expand "sync_sub<mode>"
  [(use (match_operand:GPR 0 "memory_operand" ""))
   (use (match_operand:GPR 1 "gpc_reg_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (MINUS, <MODE>mode, operands[0], operands[1], 
		    NULL_RTX, NULL_RTX, true);
  DONE;
}")

(define_expand "sync_ior<mode>"
  [(use (match_operand:INT1 0 "memory_operand" ""))
   (use (match_operand:INT1 1 "logical_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (IOR, <MODE>mode, operands[0], operands[1], 
		    NULL_RTX, NULL_RTX, true);
  DONE;
}")

(define_expand "sync_and<mode>"
  [(use (match_operand:INT1 0 "memory_operand" ""))
   (use (match_operand:INT1 1 "and_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (AND, <MODE>mode, operands[0], operands[1], 
		    NULL_RTX, NULL_RTX, true);
  DONE;
}")

(define_expand "sync_xor<mode>"
  [(use (match_operand:INT1 0 "memory_operand" ""))
   (use (match_operand:INT1 1 "logical_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (XOR, <MODE>mode, operands[0], operands[1], 
		    NULL_RTX, NULL_RTX, true);
  DONE;
}")

(define_expand "sync_nand<mode>"
  [(use (match_operand:INT1 0 "memory_operand" ""))
   (use (match_operand:INT1 1 "gpc_reg_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (AND, <MODE>mode, 
		    gen_rtx_NOT (<MODE>mode, operands[0]),
		    operands[1],
		    NULL_RTX, NULL_RTX, true);
  DONE;
}")

(define_expand "sync_old_add<mode>"
  [(use (match_operand:INT1 0 "gpc_reg_operand" ""))
   (use (match_operand:INT1 1 "memory_operand" ""))
   (use (match_operand:INT1 2 "add_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (PLUS, <MODE>mode, operands[1], operands[2], 
		    operands[0], NULL_RTX, true);
  DONE;
}")

(define_expand "sync_old_sub<mode>"
  [(use (match_operand:GPR 0 "gpc_reg_operand" ""))
   (use (match_operand:GPR 1 "memory_operand" ""))
   (use (match_operand:GPR 2 "gpc_reg_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (MINUS, <MODE>mode, operands[1], operands[2], 
		    operands[0], NULL_RTX, true);
  DONE;
}")

(define_expand "sync_old_ior<mode>"
  [(use (match_operand:INT1 0 "gpc_reg_operand" ""))
   (use (match_operand:INT1 1 "memory_operand" ""))
   (use (match_operand:INT1 2 "logical_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (IOR, <MODE>mode, operands[1], operands[2], 
		    operands[0], NULL_RTX, true);
  DONE;
}")

(define_expand "sync_old_and<mode>"
  [(use (match_operand:INT1 0 "gpc_reg_operand" ""))
   (use (match_operand:INT1 1 "memory_operand" ""))
   (use (match_operand:INT1 2 "and_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (AND, <MODE>mode, operands[1], operands[2], 
		    operands[0], NULL_RTX, true);
  DONE;
}")

(define_expand "sync_old_xor<mode>"
  [(use (match_operand:INT1 0 "gpc_reg_operand" ""))
   (use (match_operand:INT1 1 "memory_operand" ""))
   (use (match_operand:INT1 2 "logical_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (XOR, <MODE>mode, operands[1], operands[2], 
		    operands[0], NULL_RTX, true);
  DONE;
}")

(define_expand "sync_old_nand<mode>"
  [(use (match_operand:INT1 0 "gpc_reg_operand" ""))
   (use (match_operand:INT1 1 "memory_operand" ""))
   (use (match_operand:INT1 2 "gpc_reg_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (AND, <MODE>mode, 
		    gen_rtx_NOT (<MODE>mode, operands[1]),
		    operands[2],
		    operands[0], NULL_RTX, true);
  DONE;
}")

(define_expand "sync_new_add<mode>"
  [(use (match_operand:INT1 0 "gpc_reg_operand" ""))
   (use (match_operand:INT1 1 "memory_operand" ""))
   (use (match_operand:INT1 2 "add_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (PLUS, <MODE>mode, operands[1], operands[2], 
		    NULL_RTX, operands[0], true);
  DONE;
}")

(define_expand "sync_new_sub<mode>"
  [(use (match_operand:GPR 0 "gpc_reg_operand" ""))
   (use (match_operand:GPR 1 "memory_operand" ""))
   (use (match_operand:GPR 2 "gpc_reg_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (MINUS, <MODE>mode, operands[1], operands[2], 
		    NULL_RTX, operands[0], true);
  DONE;
}")

(define_expand "sync_new_ior<mode>"
  [(use (match_operand:INT1 0 "gpc_reg_operand" ""))
   (use (match_operand:INT1 1 "memory_operand" ""))
   (use (match_operand:INT1 2 "logical_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (IOR, <MODE>mode, operands[1], operands[2], 
		    NULL_RTX, operands[0], true);
  DONE;
}")

(define_expand "sync_new_and<mode>"
  [(use (match_operand:INT1 0 "gpc_reg_operand" ""))
   (use (match_operand:INT1 1 "memory_operand" ""))
   (use (match_operand:INT1 2 "and_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (AND, <MODE>mode, operands[1], operands[2], 
		    NULL_RTX, operands[0], true);
  DONE;
}")

(define_expand "sync_new_xor<mode>"
  [(use (match_operand:INT1 0 "gpc_reg_operand" ""))
   (use (match_operand:INT1 1 "memory_operand" ""))
   (use (match_operand:INT1 2 "logical_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (XOR, <MODE>mode, operands[1], operands[2], 
		    NULL_RTX, operands[0], true);
  DONE;
}")

(define_expand "sync_new_nand<mode>"
  [(use (match_operand:INT1 0 "gpc_reg_operand" ""))
   (use (match_operand:INT1 1 "memory_operand" ""))
   (use (match_operand:INT1 2 "gpc_reg_operand" ""))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "
{
  rs6000_emit_sync (AND, <MODE>mode, 
		    gen_rtx_NOT (<MODE>mode, operands[1]),
		    operands[2],
		    NULL_RTX, operands[0], true);
  DONE;
}")

; the sync_*_internal patterns all have these operands:
; 0 - memory location
; 1 - operand
; 2 - value in memory after operation
; 3 - value in memory immediately before operation

(define_insn "*sync_add<mode>_internal"
  [(set (match_operand:GPR 2 "gpc_reg_operand" "=&r,&r")
	(plus:GPR (match_operand:GPR 0 "memory_operand" "+Z,Z")
		 (match_operand:GPR 1 "add_operand" "rI,L")))
   (set (match_operand:GPR 3 "gpc_reg_operand" "=&b,&b") (match_dup 0))
   (set (match_dup 0) 
	(unspec:GPR [(plus:GPR (match_dup 0) (match_dup 1))]
		   UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 4 "=&x,&x"))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "@
   <larx> %3,%y0\n\tadd%I1 %2,%3,%1\n\t<stcx> %2,%y0\n\tbne- $-12
   <larx> %3,%y0\n\taddis %2,%3,%v1\n\t<stcx> %2,%y0\n\tbne- $-12"
  [(set_attr "length" "16,16")])

(define_insn "*sync_addshort_internal"
  [(set (match_operand:SI 2 "gpc_reg_operand" "=&r")
	(ior:SI (and:SI (plus:SI (match_operand:SI 0 "memory_operand" "+Z")
				 (match_operand:SI 1 "add_operand" "rI"))
			(match_operand:SI 4 "gpc_reg_operand" "r"))
		(and:SI (not:SI (match_dup 4)) (match_dup 0))))
   (set (match_operand:SI 3 "gpc_reg_operand" "=&b") (match_dup 0))
   (set (match_dup 0) 
	(unspec:SI [(ior:SI (and:SI (plus:SI (match_dup 0) (match_dup 1))
				    (match_dup 4))
			    (and:SI (not:SI (match_dup 4)) (match_dup 0)))]
		   UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 5 "=&x"))
   (clobber (match_scratch:SI 6 "=&r"))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "lwarx %3,%y0\n\tadd%I1 %2,%3,%1\n\tandc %6,%3,%4\n\tand %2,%2,%4\n\tor %2,%2,%6\n\tstwcx. %2,%y0\n\tbne- $-24"
  [(set_attr "length" "28")])

(define_insn "*sync_sub<mode>_internal"
  [(set (match_operand:GPR 2 "gpc_reg_operand" "=&r")
	(minus:GPR (match_operand:GPR 0 "memory_operand" "+Z")
		  (match_operand:GPR 1 "gpc_reg_operand" "r")))
   (set (match_operand:GPR 3 "gpc_reg_operand" "=&b") (match_dup 0))
   (set (match_dup 0) 
	(unspec:GPR [(minus:GPR (match_dup 0) (match_dup 1))]
		   UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 4 "=&x"))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "<larx> %3,%y0\n\tsubf %2,%1,%3\n\t<stcx> %2,%y0\n\tbne- $-12"
  [(set_attr "length" "16")])

(define_insn "*sync_andsi_internal"
  [(set (match_operand:SI 2 "gpc_reg_operand" "=&r,&r,&r,&r")
	(and:SI (match_operand:SI 0 "memory_operand" "+Z,Z,Z,Z")
		(match_operand:SI 1 "and_operand" "r,T,K,L")))
   (set (match_operand:SI 3 "gpc_reg_operand" "=&b,&b,&b,&b") (match_dup 0))
   (set (match_dup 0) 
	(unspec:SI [(and:SI (match_dup 0) (match_dup 1))]
		   UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 4 "=&x,&x,&x,&x"))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "@
   lwarx %3,%y0\n\tand %2,%3,%1\n\tstwcx. %2,%y0\n\tbne- $-12
   lwarx %3,%y0\n\trlwinm %2,%3,0,%m1,%M1\n\tstwcx. %2,%y0\n\tbne- $-12
   lwarx %3,%y0\n\tandi. %2,%3,%b1\n\tstwcx. %2,%y0\n\tbne- $-12
   lwarx %3,%y0\n\tandis. %2,%3,%u1\n\tstwcx. %2,%y0\n\tbne- $-12"
  [(set_attr "length" "16,16,16,16")])

(define_insn "*sync_anddi_internal"
  [(set (match_operand:DI 2 "gpc_reg_operand" "=&r,&r,&r,&r,&r")
	(and:DI (match_operand:DI 0 "memory_operand" "+Z,Z,Z,Z,Z")
		(match_operand:DI 1 "and_operand" "r,S,T,K,J")))
   (set (match_operand:DI 3 "gpc_reg_operand" "=&b,&b,&b,&b,&b") (match_dup 0))
   (set (match_dup 0) 
	(unspec:DI [(and:DI (match_dup 0) (match_dup 1))]
		   UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 4 "=&x,&x,&x,&x,&x"))]
  "TARGET_POWERPC64"
  "@
   ldarx %3,%y0\n\tand %2,%3,%1\n\tstdcx. %2,%y0\n\tbne- $-12
   ldarx %3,%y0\n\trldic%B1 %2,%3,0,%S1\n\tstdcx. %2,%y0\n\tbne- $-12
   ldarx %3,%y0\n\trlwinm %2,%3,0,%m1,%M1\n\tstdcx. %2,%y0\n\tbne- $-12
   ldarx %3,%y0\n\tandi. %2,%3,%b1\n\tstdcx. %2,%y0\n\tbne- $-12
   ldarx %3,%y0\n\tandis. %2,%3,%b1\n\tstdcx. %2,%y0\n\tbne- $-12"
  [(set_attr "length" "16,16,16,16,16")])

(define_insn "*sync_boolsi_internal"
  [(set (match_operand:SI 2 "gpc_reg_operand" "=&r,&r,&r")
	(match_operator:SI 4 "boolean_or_operator"
	 [(match_operand:SI 0 "memory_operand" "+Z,Z,Z")
	  (match_operand:SI 1 "logical_operand" "r,K,L")]))
   (set (match_operand:SI 3 "gpc_reg_operand" "=&b,&b,&b") (match_dup 0))
   (set (match_dup 0) (unspec:SI [(match_dup 4)] UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 5 "=&x,&x,&x"))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "@
   lwarx %3,%y0\n\t%q4 %2,%3,%1\n\tstwcx. %2,%y0\n\tbne- $-12
   lwarx %3,%y0\n\t%q4i %2,%3,%b1\n\tstwcx. %2,%y0\n\tbne- $-12
   lwarx %3,%y0\n\t%q4is %2,%3,%u1\n\tstwcx. %2,%y0\n\tbne- $-12"
  [(set_attr "length" "16,16,16")])

(define_insn "*sync_booldi_internal"
  [(set (match_operand:DI 2 "gpc_reg_operand" "=&r,&r,&r")
	(match_operator:DI 4 "boolean_or_operator"
	 [(match_operand:DI 0 "memory_operand" "+Z,Z,Z")
	  (match_operand:DI 1 "logical_operand" "r,K,JF")]))
   (set (match_operand:DI 3 "gpc_reg_operand" "=&b,&b,&b") (match_dup 0))
   (set (match_dup 0) (unspec:DI [(match_dup 4)] UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 5 "=&x,&x,&x"))]
  "TARGET_POWERPC64"
  "@
   ldarx %3,%y0\n\t%q4 %2,%3,%1\n\tstdcx. %2,%y0\n\tbne- $-12
   ldarx %3,%y0\n\t%q4i %2,%3,%b1\n\tstdcx. %2,%y0\n\tbne- $-12
   ldarx %3,%y0\n\t%q4is %2,%3,%u1\n\tstdcx. %2,%y0\n\tbne- $-12"
  [(set_attr "length" "16,16,16")])

(define_insn "*sync_boolc<mode>_internal"
  [(set (match_operand:GPR 2 "gpc_reg_operand" "=&r")
	(match_operator:GPR 4 "boolean_operator"
	 [(not:GPR (match_operand:GPR 0 "memory_operand" "+Z"))
	  (match_operand:GPR 1 "gpc_reg_operand" "r")]))
   (set (match_operand:GPR 3 "gpc_reg_operand" "=&b") (match_dup 0))
   (set (match_dup 0) (unspec:GPR [(match_dup 4)] UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 5 "=&x"))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "<larx> %3,%y0\n\t%q4 %2,%1,%3\n\t<stcx> %2,%y0\n\tbne- $-12"
  [(set_attr "length" "16")])

; This pattern could also take immediate values of operand 1,
; since the non-NOT version of the operator is used; but this is not
; very useful, since in practice operand 1 is a full 32-bit value.
; Likewise, operand 5 is in practice either <= 2^16 or it is a register.
(define_insn "*sync_boolcshort_internal"
  [(set (match_operand:SI 2 "gpc_reg_operand" "=&r")
	(match_operator:SI 4 "boolean_operator"
	 [(xor:SI (match_operand:SI 0 "memory_operand" "+Z")
		  (match_operand:SI 5 "logical_operand" "rK"))
	  (match_operand:SI 1 "gpc_reg_operand" "r")]))
   (set (match_operand:SI 3 "gpc_reg_operand" "=&b") (match_dup 0))
   (set (match_dup 0) (unspec:SI [(match_dup 4)] UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 6 "=&x"))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "lwarx %3,%y0\n\txor%I2 %2,%3,%5\n\t%q4 %2,%2,%1\n\tstwcx. %2,%y0\n\tbne- $-16"
  [(set_attr "length" "20")])

(define_insn "*sync_boolc<mode>_internal2"
  [(set (match_operand:GPR 2 "gpc_reg_operand" "=&r")
	(match_operator:GPR 4 "boolean_operator"
	 [(not:GPR (match_operand:GPR 1 "gpc_reg_operand" "r"))
	  (match_operand:GPR 0 "memory_operand" "+Z")]))
   (set (match_operand:GPR 3 "gpc_reg_operand" "=&b") (match_dup 0))
   (set (match_dup 0) (unspec:GPR [(match_dup 4)] UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 5 "=&x"))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "<larx> %3,%y0\n\t%q4 %2,%3,%1\n\t<stcx> %2,%y0\n\tbne- $-12"
  [(set_attr "length" "16")])

(define_insn "*sync_boolcc<mode>_internal"
  [(set (match_operand:GPR 2 "gpc_reg_operand" "=&r")
	(match_operator:GPR 4 "boolean_operator"
	 [(not:GPR (match_operand:GPR 0 "memory_operand" "+Z"))
	  (not:GPR (match_operand:GPR 1 "gpc_reg_operand" "r"))]))
   (set (match_operand:GPR 3 "gpc_reg_operand" "=&b") (match_dup 0))
   (set (match_dup 0) (unspec:GPR [(match_dup 4)] UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 5 "=&x"))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "<larx> %3,%y0\n\t%q4 %2,%1,%3\n\t<stcx> %2,%y0\n\tbne- $-12"
  [(set_attr "length" "16")])

(define_insn "isync"
  [(set (mem:BLK (match_scratch 0 "X"))
        (unspec:BLK [(mem:BLK (match_scratch 1 "X"))] UNSPEC_ISYNC))]
  "TARGET_POWERPC"
  "isync")

(define_insn "sync_lock_test_and_set<mode>"
  [(set (match_operand:GPR 0 "gpc_reg_operand" "=&r")
	(match_operand:GPR 1 "memory_operand" "+Z"))
   (set (match_dup 1) (unspec:GPR [(match_operand:GPR 2 "gpc_reg_operand" "r")] 
				 UNSPEC_SYNC_OP))
   (clobber (match_scratch:CC 3 "=&x"))
   (set (mem:BLK (match_scratch 4 "X"))
        (unspec:BLK [(mem:BLK (match_scratch 5 "X"))] UNSPEC_ISYNC))]
  "TARGET_POWERPC && !PPC405_ERRATUM77"
  "<larx> %0,%y1\n\t<stcx> %2,%y1\n\tbne- $-8\n\tisync"
  [(set_attr "length" "16")])

(define_expand "sync_lock_release<mode>"
  [(set (match_operand:INT 0 "memory_operand")
	(match_operand:INT 1 "any_operand"))]
  ""
  "
{
  emit_insn (gen_lwsync ());
  emit_move_insn (operands[0], operands[1]);
  DONE;
}")

; Some AIX assemblers don't accept lwsync, so we use a .long.
(define_insn "lwsync"
  [(set (mem:BLK (match_scratch 0 "X"))
        (unspec:BLK [(mem:BLK (match_scratch 1 "X"))] UNSPEC_LWSYNC))]
  ""
  ".long 0x7c2004ac")

