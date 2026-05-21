# Makefile — one4all unified build
#
# Primary targets:
#   make scrip        — build the unified scrip x86 executable
#   make all          — alias for scrip
#   make setup        — install system packages + CSNOBOL4 + SPITBOL oracle
#   make test         — run corpus (--sm-run, PASS=178 gate)
#   make test-ir      — run corpus (--ir-run mode)
#   make test-all     — both passes back-to-back
#   make monitor-ipc  — build test/monitor/monitor_ipc.so
#   make clean        — remove build artefacts
#   make distclean    — clean + remove /tmp caches
#
# Runner wrappers (run a single .sno file):
#   make run SNO=file.sno              — default (--sm-run)
#   make run-ir SNO=file.sno           — --ir-run (IR tree-walk)
#   make run-jvm SNO=file.sno          — legacy JVM (until M-JITEM-JVM)
#   make run-net SNO=file.sno          — legacy .NET (until M-JITEM-NET)
#
# Note: run-asm retired — replaced by: scrip --jit-emit --x64 (M-JITEM-X64)
#
# Prerequisites:
#   apt-get install -y libgc-dev flex nasm build-essential libgmp-dev m4
#
# Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6

ROOT    := $(shell pwd)
SRC     := $(ROOT)/src
RT      := $(SRC)/runtime
BOXES   := $(SRC)/processor
CORPUS  ?= $(ROOT)/../corpus
OBJ     := /tmp/si_objs
CC      := gcc
WARN    := -w
CBASE   := -O0 -g $(WARN) -I$(SRC) -I$(SRC)/include -I$(SRC)/lower -I$(SRC)/processor -I$(SRC)/emitter -I$(SRC)/runtime/snobol4 -I$(RT)
CRT     := $(CBASE) -DDYN_ENGINE_LINKED
LIBS    := -lgc -lm

# Runner defaults
SNO          ?= $(error SNO is required — e.g. make run SNO=prog.sno)
INC          ?= $(CORPUS)/programs/inc
JVM_CACHE    := /tmp/scrip_jvm_cache
NET_CACHE    := /tmp/scrip_net_cache
JASMIN       := $(SRC)/backend/jasmin.jar
SCRIP_CC_BIN := $(ROOT)/scrip

.PHONY: all scrip scrip-interp scrip setup \
        test test-ir test-all \
        jit-emit-test \
        monitor-ipc \
        libscrip_rt \
        run run-ir run-jvm run-net \
        clean distclean

# ── Primary target ────────────────────────────────────────────────────────────

all: scrip

# ── libscrip_rt.so — runtime support library for --jit-emit --x64 ────────────
# EM-6: full SNOBOL4 runtime compiled -fPIC and linked into the .so.
# Emitted x86-64 binaries link against this .so for all language-level
# semantics (pattern matcher, NV table, exec_stmt, builtins, GC).
libscrip_rt: out/libscrip_rt.so

# EM-6 runtime objects (all compiled -fPIC so they can go into the .so)
RT_PIC_SRCS := \
    $(RT)/rt/rt.c \
    $(SRC)/runtime/snobol4/snobol4.c \
    $(SRC)/runtime/snobol4/snobol4_pattern.c \
    $(SRC)/runtime/snobol4/snobol4_invoke.c \
    $(SRC)/runtime/snobol4/snobol4_argval.c \
    $(SRC)/runtime/snobol4/snobol4_nmd.c \
    $(SRC)/runtime/snobol4/name_t.c \
    $(SRC)/runtime/snobol4/stmt_exec.c \
    $(SRC)/runtime/snobol4/eval_code.c \
    $(SRC)/runtime/snobol4/eval_pat.c \
    $(SRC)/processor/bb_pool.c \
    $(SRC)/emitter/emit_core.c \
    $(SRC)/emitter/emit_globals.c \
    $(SRC)/emitter/emit_io.c \
    $(SRC)/emitter/BB_templates/bb_abort.c \
    $(SRC)/emitter/BB_templates/bb_alt.c \
    $(SRC)/emitter/BB_templates/bb_any.c \
    $(SRC)/emitter/BB_templates/bb_arb.c \
    $(SRC)/emitter/BB_templates/bb_arbno.c \
    $(SRC)/emitter/BB_templates/bb_break.c \
    $(SRC)/emitter/BB_templates/bb_capture.c \
    $(SRC)/emitter/BB_templates/bb_cat.c \
    $(SRC)/emitter/BB_templates/bb_charset_helper.c \
    $(SRC)/emitter/BB_templates/bb_dsar.c \
    $(SRC)/emitter/BB_templates/bb_atp_template.c \
    $(SRC)/emitter/BB_templates/bb_fence.c \
    $(SRC)/emitter/BB_templates/bb_len.c \
    $(SRC)/emitter/BB_templates/bb_lit.c \
    $(SRC)/emitter/BB_templates/bb_notany.c \
    $(SRC)/emitter/BB_templates/bb_pl_arith.c \
    $(SRC)/emitter/BB_templates/bb_pl_atom.c \
    $(SRC)/emitter/BB_templates/bb_pl_builtin.c \
    $(SRC)/emitter/BB_templates/bb_pl_call.c \
    $(SRC)/emitter/BB_templates/bb_lit_i.c \
    $(SRC)/emitter/BB_templates/bb_lit_s.c \
    $(SRC)/emitter/BB_templates/bb_lit_f.c \
    $(SRC)/emitter/BB_templates/bb_lit_nul.c \
    $(SRC)/emitter/BB_templates/bb_var.c \
    $(SRC)/emitter/BB_templates/bb_assign.c \
    $(SRC)/emitter/BB_templates/bb_augop.c \
    $(SRC)/emitter/BB_templates/bb_binop.c \
    $(SRC)/emitter/BB_templates/bb_unop.c \
    $(SRC)/emitter/BB_templates/bb_call.c \
    $(SRC)/emitter/BB_templates/bb_seq.c \
    $(SRC)/emitter/BB_templates/bb_fail.c \
    $(SRC)/emitter/BB_templates/bb_succeed.c \
    $(SRC)/emitter/BB_templates/bb_goto.c \
    $(SRC)/emitter/BB_templates/bb_return.c \
    $(SRC)/emitter/BB_templates/bb_if.c \
    $(SRC)/emitter/BB_templates/bb_alternate.c \
    $(SRC)/emitter/BB_templates/bb_to_by.c \
    $(SRC)/emitter/BB_templates/bb_every.c \
    $(SRC)/emitter/BB_templates/bb_while.c \
    $(SRC)/emitter/BB_templates/bb_until.c \
    $(SRC)/emitter/BB_templates/bb_repeat.c \
    $(SRC)/emitter/BB_templates/bb_ctl_alt.c \
    $(SRC)/emitter/BB_templates/bb_size.c \
    $(SRC)/emitter/BB_templates/bb_case.c \
    $(SRC)/emitter/BB_templates/bb_limit.c \
    $(SRC)/emitter/BB_templates/bb_suspend.c \
    $(SRC)/emitter/BB_templates/bb_proc.c \
    $(SRC)/emitter/BB_templates/bb_scan.c \
    $(SRC)/emitter/BB_templates/bb_nonnull.c \
    $(SRC)/emitter/BB_templates/bb_interrogate.c \
    $(SRC)/emitter/BB_templates/bb_not.c \
    $(SRC)/emitter/BB_templates/bb_pat_callout.c \
    $(SRC)/emitter/BB_templates/bb_pl_choice.c \
    $(SRC)/emitter/BB_templates/bb_pl_unify.c \
    $(SRC)/emitter/BB_templates/bb_pl_cut.c \
    $(SRC)/emitter/BB_templates/bb_pl_var.c \
    $(SRC)/emitter/BB_templates/bb_pl_alt.c \
    $(SRC)/emitter/BB_templates/bb_pl_seq.c \
    $(SRC)/emitter/BB_templates/bb_icn_to.c \
    $(SRC)/emitter/BB_templates/bb_icn_upto.c \
    $(SRC)/emitter/BB_templates/bb_icn_to_by.c \
    $(SRC)/emitter/BB_templates/bb_icn_iterate.c \
    $(SRC)/emitter/BB_templates/bb_icn_alternate.c \
    $(SRC)/emitter/BB_templates/bb_icn_limit.c \
    $(SRC)/emitter/BB_templates/bb_icn_binop.c \
    $(SRC)/emitter/BB_templates/bb_icn_to_nested.c \
    $(SRC)/emitter/BB_templates/bb_icn_proc_gen.c \
    $(SRC)/emitter/BB_templates/bb_ctl_break.c \
    $(SRC)/emitter/BB_templates/bb_next.c \
    $(SRC)/emitter/BB_templates/bb_identical.c \
    $(SRC)/emitter/BB_templates/bb_null_test.c \
    $(SRC)/emitter/BB_templates/bb_random.c \
    $(SRC)/emitter/BB_templates/bb_neg.c \
    $(SRC)/emitter/BB_templates/bb_ctl_pos.c \
    $(SRC)/emitter/BB_templates/bb_cset_compl.c \
    $(SRC)/emitter/BB_templates/bb_cset_union.c \
    $(SRC)/emitter/BB_templates/bb_cset_diff.c \
    $(SRC)/emitter/BB_templates/bb_cset_inter.c \
    $(SRC)/emitter/BB_templates/bb_icn_scan.c \
    $(SRC)/emitter/BB_templates/bb_icn_keyword.c \
    $(SRC)/emitter/BB_templates/bb_binop_gen.c \
    $(SRC)/emitter/BB_templates/bb_icn_idx.c \
    $(SRC)/emitter/BB_templates/bb_icn_section.c \
    $(SRC)/emitter/BB_templates/bb_icn_list_bang.c \
    $(SRC)/emitter/BB_templates/bb_icn_record_def.c \
    $(SRC)/emitter/BB_templates/bb_icn_field_get.c \
    $(SRC)/emitter/BB_templates/bb_icn_field_set.c \
    $(SRC)/emitter/BB_templates/bb_icn_idx_set.c \
    $(SRC)/emitter/BB_templates/bb_icn_key_gen.c \
    $(SRC)/emitter/BB_templates/bb_swap.c \
    $(SRC)/emitter/BB_templates/bb_seq_expr.c \
    $(SRC)/emitter/BB_templates/bb_initial.c \
    $(SRC)/emitter/BB_templates/bb_icn_lconcat.c \
    $(SRC)/emitter/BB_templates/bb_icn_find_gen.c \
    $(SRC)/emitter/BB_templates/bb_icn_seq_gen.c \
    $(SRC)/emitter/BB_templates/bb_pos.c \
    $(SRC)/emitter/BB_templates/bb_rem.c \
    $(SRC)/emitter/BB_templates/bb_span.c \
    $(SRC)/emitter/BB_templates/bb_tab.c \
    $(SRC)/emitter/SM_templates/sm_arith.c \
    $(SRC)/emitter/SM_templates/sm_bb_calls.c \
    $(SRC)/emitter/SM_templates/sm_calls.c \
    $(SRC)/emitter/SM_templates/sm_compare.c \
    $(SRC)/emitter/SM_templates/sm_defines.c \
    $(SRC)/emitter/SM_templates/sm_expr_incr.c \
    $(SRC)/emitter/SM_templates/sm_halt.c \
    $(SRC)/emitter/SM_templates/sm_jumps.c \
    $(SRC)/emitter/SM_templates/sm_pat_anchors.c \
    $(SRC)/emitter/SM_templates/sm_pat_combine.c \
    $(SRC)/emitter/SM_templates/sm_pat_control.c \
    $(SRC)/emitter/SM_templates/sm_pat_position.c \
    $(SRC)/emitter/SM_templates/sm_push_pop_lits.c \
    $(SRC)/emitter/SM_templates/sm_returns.c \
    $(SRC)/emitter/emit_bb.c \
    $(SRC)/emitter/emit_sm.c \
    \
    $(SRC)/processor/bb_boxes.c \
    $(SRC)/processor/bb_broker.c \
    $(SRC)/lower/sm_prog.c \
    $(SRC)/processor/sm_interp.c \
    $(SRC)/lower/lower.c \
    $(SRC)/lower/lower_ctx.c \
    $(SRC)/lower/lower_pat_dcg.c \
    $(SRC)/lower/lower_icn.c \
    $(SRC)/lower/lower_pl.c \
    $(SRC)/lower/lower_sno.c \
    $(SRC)/processor/sm_image.c \
    $(SRC)/processor/sm_jit_interp.c \
    $(SRC)/runtime/interp/icn_runtime.c \
    $(SRC)/runtime/interp/scan_builtins.c \
    $(SRC)/runtime/interp/raku_builtins.c \
    $(SRC)/runtime/interp/pl_runtime.c \
    $(SRC)/runtime/interp/icon_box_rt.c \
    $(SRC)/runtime/snobol4/coerce.c \
    $(SRC)/lower/ast_clone.c \
    $(SRC)/lower/scrip_ir.c \
    $(SRC)/lower/ir_exec.c \
    $(SRC)/driver/interp_globals.c \
    $(SRC)/driver/interp_label.c \
    $(SRC)/driver/interp_hooks.c \
    $(SRC)/driver/interp_data.c \
    $(SRC)/driver/interp_call.c \
    $(SRC)/driver/interp_ref.c \
    $(SRC)/driver/interp_ast_stubs.c \
    $(SRC)/driver/scrip_sm.c \
    $(SRC)/driver/stmt_ast.c \
    $(SRC)/driver/sync_monitor.c \
    $(SRC)/driver/polyglot.c \
    $(SRC)/ast/ast_print.c \
    $(SRC)/frontend/snobol4/snobol4.tab.c \
    $(SRC)/frontend/snobol4/snobol4.lex.c \
    $(SRC)/frontend/icon/icon_runtime.c \
    $(SRC)/frontend/icon/icon_parse.c \
    $(SRC)/frontend/icon/icon_lex.c \
    $(SRC)/frontend/icon/icon_driver.c \
    $(SRC)/frontend/prolog/prolog_lex.c \
    $(SRC)/frontend/prolog/prolog_parse.c \
    $(SRC)/frontend/prolog/prolog_lower.c \
    $(SRC)/frontend/prolog/prolog_atom.c \
    $(SRC)/frontend/prolog/prolog_builtin.c \
    $(SRC)/frontend/prolog/prolog_unify.c \
    $(SRC)/frontend/prolog/prolog_driver.c \
    $(SRC)/frontend/prolog/pl_broker.c \
    $(SRC)/frontend/snocone/snocone_lex.c \
    $(SRC)/frontend/snocone/snocone_parse.tab.c \
    $(SRC)/frontend/snocone/snocone_driver.c \
    $(SRC)/frontend/raku/raku.tab.c \
    $(SRC)/frontend/raku/raku.lex.c \
    $(SRC)/frontend/raku/raku_driver.c \
    $(SRC)/frontend/raku/raku_re.c \
    $(SRC)/frontend/rebus/rebus.tab.c \
    $(SRC)/frontend/rebus/lex.rebus.c \
    $(SRC)/frontend/rebus/rebus_lower.c \
    $(SRC)/frontend/rebus/rebus_emit.c \
    $(SRC)/frontend/rebus/rebus_print.c

out/libscrip_rt.so: $(RT_PIC_SRCS) $(RT)/rt/rt.h
	@mkdir -p out
	$(CC) -O0 -g $(WARN) -fPIC -shared \
	    -I$(SRC) -I$(SRC)/include -I$(SRC)/lower -I$(SRC)/processor -I$(SRC)/emitter -I$(SRC)/runtime/snobol4 -I$(RT) -I$(RT)/rt \
	    -I$(SRC)/frontend/snobol4 -I$(SRC)/frontend/raku \
	    -DDYN_ENGINE_LINKED -DIR_DEFINE_NAMES \
	    $(RT_PIC_SRCS) \
	    -lgc -lm \
	    -o out/libscrip_rt.so
	@echo "Built: out/libscrip_rt.so"

# ── EM-2 synthetic-program harness ───────────────────────────────────────────
# Standalone helper: builds a 3-op SM_Program in memory and emits asm
# via sm_codegen_x64_emit().  The shell gate then assembles/links/runs.
out/sm_codegen_x64_emit_test: $(SRC)/emitter/sm_codegen_x64_emit_test.c \
                                \
                               $(SRC)/lower/sm_prog.c \
                               $(SRC)/lower/sm_prog.h \
                               out/libscrip_rt.so
	@mkdir -p out
	$(CC) -O0 -g $(WARN) \
	    -I$(SRC) -I$(SRC)/lower -I$(SRC)/processor -I$(SRC)/emitter -I$(SRC)/runtime/snobol4 -I$(RT) -I$(RT)/rt \
	    -DDYN_ENGINE_LINKED \
	    $(SRC)/emitter/sm_codegen_x64_emit_test.c \
	    $(SRC)/lower/sm_prog.c \
	    -Lout -lscrip_rt -lgc -lm \
	    -Wl,-rpath,$(shell pwd)/out \
	    -o out/sm_codegen_x64_emit_test
	@echo "Built: out/sm_codegen_x64_emit_test"


# ── EM-7a/EM-7b unit tests retired 2026-05-19 (EC-BB-UNIFY-2): the underlying
# compile-time PATND_t API converted to IR_t*. The tests asserted XKIND_t
# fields (kind/nchildren/children) and the patnd_is_fully_invariant(PATND_t*)
# signature, both of which no longer exist for the compile-time walker.
# Coverage subsumed by GATE-2 (broker) and the mode-4 compile gate.

# ── scrip — unified driver (all modes, all frontends) ────────────────────────
# WASM removed from scrip build (2026-04-08): --jit-emit --wasm / emit_wasm.c
# dropped. Use scrip legacy driver if WASM emission is ever needed.

scrip:
	@mkdir -p $(OBJ)
	@rm -f $(OBJ)/*.o
	$(CC) $(CBASE) -c $(SRC)/frontend/snobol4/snobol4.lex.c -o $(OBJ)/snobol4.lex.o
	$(CC) $(CBASE) -c $(SRC)/frontend/snobol4/snobol4.tab.c -o $(OBJ)/snobol4.tab.o
	$(CC) $(CRT)   -c $(SRC)/runtime/snobol4/snobol4.c               -o $(OBJ)/snobol4.o
	$(CC) $(CRT)   -c $(SRC)/runtime/snobol4/snobol4_pattern.c        -o $(OBJ)/snobol4_pattern.o
	$(CC) $(CRT)   -c $(SRC)/runtime/snobol4/snobol4_invoke.c                 -o $(OBJ)/snobol4_invoke.o
	$(CC) $(CRT)   -c $(SRC)/runtime/snobol4/snobol4_argval.c                 -o $(OBJ)/snobol4_argval.o
	$(CC) $(CRT)   -c $(SRC)/runtime/snobol4/snobol4_nmd.c                    -o $(OBJ)/snobol4_nmd.o
	$(CC) $(CRT)   -c $(SRC)/runtime/snobol4/name_t.c                         -o $(OBJ)/name_t.o
	$(CC) $(CRT)   -c $(SRC)/runtime/snobol4/stmt_exec.c                  -o $(OBJ)/stmt_exec.o
	$(CC) $(CRT)   -c $(SRC)/runtime/snobol4/eval_code.c                  -o $(OBJ)/eval_code.o
	$(CC) $(CRT)   -c $(SRC)/runtime/snobol4/eval_pat.c                   -o $(OBJ)/eval_pat.o
	$(CC) $(CRT)   -c $(SRC)/processor/bb_pool.c                    -o $(OBJ)/bb_pool.o
	$(CC) $(CRT)   -c $(SRC)/emitter/emit_core.c               -o $(OBJ)/emit_core.o
	$(CC) $(CRT)   -c $(SRC)/emitter/emit_globals.c            -o $(OBJ)/emit_globals.o
	$(CC) $(CRT)   -c $(SRC)/emitter/emit_io.c                 -o $(OBJ)/emit_io.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_abort.c       -o $(OBJ)/bb_abort.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_alt.c         -o $(OBJ)/bb_alt.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_any.c         -o $(OBJ)/bb_any.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_arb.c         -o $(OBJ)/bb_arb.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_arbno.c       -o $(OBJ)/bb_arbno.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_break.c       -o $(OBJ)/bb_break.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_capture.c     -o $(OBJ)/bb_capture.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_cat.c         -o $(OBJ)/bb_cat.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_charset_helper.c -o $(OBJ)/bb_charset_helper.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_dsar.c        -o $(OBJ)/bb_dsar.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_atp_template.c         -o $(OBJ)/bb_atp_template.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_fence.c       -o $(OBJ)/bb_fence.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_len.c         -o $(OBJ)/bb_len.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_lit.c         -o $(OBJ)/bb_lit.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_notany.c      -o $(OBJ)/bb_notany.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pl_arith.c    -o $(OBJ)/bb_pl_arith.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pl_atom.c     -o $(OBJ)/bb_pl_atom.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pl_builtin.c  -o $(OBJ)/bb_pl_builtin.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pl_call.c     -o $(OBJ)/bb_pl_call.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_lit_i.c             -o $(OBJ)/bb_lit_i.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_lit_s.c             -o $(OBJ)/bb_lit_s.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_lit_f.c             -o $(OBJ)/bb_lit_f.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_lit_nul.c           -o $(OBJ)/bb_lit_nul.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_var.c               -o $(OBJ)/bb_var.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_assign.c            -o $(OBJ)/bb_assign.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_augop.c             -o $(OBJ)/bb_augop.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_binop.c             -o $(OBJ)/bb_binop.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_unop.c              -o $(OBJ)/bb_unop.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_call.c              -o $(OBJ)/bb_call.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_seq.c               -o $(OBJ)/bb_seq.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_fail.c              -o $(OBJ)/bb_fail.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_succeed.c           -o $(OBJ)/bb_succeed.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_goto.c              -o $(OBJ)/bb_goto.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_return.c            -o $(OBJ)/bb_return.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_if.c                -o $(OBJ)/bb_if.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_alternate.c         -o $(OBJ)/bb_alternate.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_to_by.c             -o $(OBJ)/bb_to_by.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_every.c             -o $(OBJ)/bb_every.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_while.c             -o $(OBJ)/bb_while.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_until.c             -o $(OBJ)/bb_until.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_repeat.c            -o $(OBJ)/bb_repeat.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_ctl_alt.c           -o $(OBJ)/bb_ctl_alt.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_size.c              -o $(OBJ)/bb_size.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_case.c              -o $(OBJ)/bb_case.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_limit.c             -o $(OBJ)/bb_limit.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_suspend.c           -o $(OBJ)/bb_suspend.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_proc.c              -o $(OBJ)/bb_proc.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_scan.c              -o $(OBJ)/bb_scan.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_nonnull.c           -o $(OBJ)/bb_nonnull.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_interrogate.c       -o $(OBJ)/bb_interrogate.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_not.c               -o $(OBJ)/bb_not.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pat_callout.c       -o $(OBJ)/bb_pat_callout.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pl_choice.c         -o $(OBJ)/bb_pl_choice.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pl_unify.c          -o $(OBJ)/bb_pl_unify.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pl_cut.c            -o $(OBJ)/bb_pl_cut.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pl_var.c            -o $(OBJ)/bb_pl_var.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pl_alt.c            -o $(OBJ)/bb_pl_alt.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pl_seq.c            -o $(OBJ)/bb_pl_seq.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_to.c            -o $(OBJ)/bb_icn_to.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_upto.c          -o $(OBJ)/bb_icn_upto.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_to_by.c         -o $(OBJ)/bb_icn_to_by.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_iterate.c       -o $(OBJ)/bb_icn_iterate.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_alternate.c     -o $(OBJ)/bb_icn_alternate.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_limit.c         -o $(OBJ)/bb_icn_limit.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_binop.c         -o $(OBJ)/bb_icn_binop.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_to_nested.c     -o $(OBJ)/bb_icn_to_nested.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_proc_gen.c      -o $(OBJ)/bb_icn_proc_gen.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_ctl_break.c         -o $(OBJ)/bb_ctl_break.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_next.c              -o $(OBJ)/bb_next.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_identical.c         -o $(OBJ)/bb_identical.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_null_test.c         -o $(OBJ)/bb_null_test.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_random.c            -o $(OBJ)/bb_random.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_neg.c               -o $(OBJ)/bb_neg.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_ctl_pos.c           -o $(OBJ)/bb_ctl_pos.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_cset_compl.c        -o $(OBJ)/bb_cset_compl.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_cset_union.c        -o $(OBJ)/bb_cset_union.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_cset_diff.c         -o $(OBJ)/bb_cset_diff.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_cset_inter.c        -o $(OBJ)/bb_cset_inter.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_scan.c          -o $(OBJ)/bb_icn_scan.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_keyword.c       -o $(OBJ)/bb_icn_keyword.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_binop_gen.c         -o $(OBJ)/bb_binop_gen.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_idx.c           -o $(OBJ)/bb_icn_idx.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_section.c       -o $(OBJ)/bb_icn_section.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_list_bang.c     -o $(OBJ)/bb_icn_list_bang.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_record_def.c    -o $(OBJ)/bb_icn_record_def.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_field_get.c     -o $(OBJ)/bb_icn_field_get.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_field_set.c     -o $(OBJ)/bb_icn_field_set.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_idx_set.c       -o $(OBJ)/bb_icn_idx_set.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_key_gen.c       -o $(OBJ)/bb_icn_key_gen.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_swap.c              -o $(OBJ)/bb_swap.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_seq_expr.c          -o $(OBJ)/bb_seq_expr.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_initial.c           -o $(OBJ)/bb_initial.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_lconcat.c       -o $(OBJ)/bb_icn_lconcat.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_find_gen.c      -o $(OBJ)/bb_icn_find_gen.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_icn_seq_gen.c       -o $(OBJ)/bb_icn_seq_gen.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_pos.c         -o $(OBJ)/bb_pos.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_rem.c         -o $(OBJ)/bb_rem.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_span.c        -o $(OBJ)/bb_span.o
	$(CC) $(CRT)   -c $(SRC)/emitter/BB_templates/bb_tab.c         -o $(OBJ)/bb_tab.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_arith.c -o $(OBJ)/sm_arith.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_bb_calls.c -o $(OBJ)/sm_bb_calls.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_calls.c -o $(OBJ)/sm_calls.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_compare.c -o $(OBJ)/sm_compare.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_defines.c -o $(OBJ)/sm_defines.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_expr_incr.c -o $(OBJ)/sm_expr_incr.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_halt.c -o $(OBJ)/sm_halt.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_jumps.c -o $(OBJ)/sm_jumps.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_pat_anchors.c -o $(OBJ)/sm_pat_anchors.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_pat_combine.c -o $(OBJ)/sm_pat_combine.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_pat_control.c -o $(OBJ)/sm_pat_control.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_pat_position.c -o $(OBJ)/sm_pat_position.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_push_pop_lits.c -o $(OBJ)/sm_push_pop_lits.o
	$(CC) $(CRT)   -I$(SRC)/emitter/SM_templates -c $(SRC)/emitter/SM_templates/sm_returns.c -o $(OBJ)/sm_returns.o
	$(CC) $(CRT) -c $(SRC)/processor/bb_boxes.c -o $(OBJ)/bb_boxes.o
	$(CC) $(CRT) -c $(SRC)/processor/bb_broker.c -o $(OBJ)/bb_broker.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -DIR_DEFINE_NAMES \
	    -c $(SRC)/ast/ast_print.c -o $(OBJ)/ast_print.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/snocone/snocone_lex.c        -o $(OBJ)/snocone_lex.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/snocone/snocone_parse.tab.c  -o $(OBJ)/snocone_parse.tab.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/snocone/snocone_driver.c     -o $(OBJ)/snocone_driver.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_lex.c      -o $(OBJ)/prolog_lex.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_parse.c    -o $(OBJ)/prolog_parse.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_lower.c    -o $(OBJ)/prolog_lower.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_atom.c     -o $(OBJ)/prolog_atom.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_builtin.c  -o $(OBJ)/prolog_builtin.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_unify.c    -o $(OBJ)/prolog_unify.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/prolog_driver.c   -o $(OBJ)/prolog_driver.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/prolog/pl_broker.c       -o $(OBJ)/pl_broker.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/icon/icon_lex.c         -o $(OBJ)/icon_lex.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/icon/icon_parse.c       -o $(OBJ)/icon_parse.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/icon/icon_runtime.c     -o $(OBJ)/icon_runtime.o

	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/icon/icon_driver.c      -o $(OBJ)/icon_driver.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -I$(SRC)/frontend/raku -c $(SRC)/frontend/raku/raku.tab.c    -o $(OBJ)/raku.tab.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -I$(SRC)/frontend/raku -c $(SRC)/frontend/raku/raku.lex.c    -o $(OBJ)/raku.lex.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -I$(SRC)/frontend/raku -c $(SRC)/frontend/raku/raku_driver.c -o $(OBJ)/raku_driver.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -I$(SRC)/frontend/raku -c $(SRC)/frontend/raku/raku_re.c      -o $(OBJ)/raku_re.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/rebus/rebus.tab.c    -o $(OBJ)/rebus.tab.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/rebus/lex.rebus.c    -o $(OBJ)/lex.rebus.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/rebus/rebus_lower.c  -o $(OBJ)/rebus_lower.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/rebus/rebus_emit.c   -o $(OBJ)/rebus_emit.o
	$(CC) $(CBASE) -I$(SRC)/frontend/snobol4 -c $(SRC)/frontend/rebus/rebus_print.c  -o $(OBJ)/rebus_print.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/icn_runtime.c -o $(OBJ)/icn_runtime.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/raku_builtins.c -o $(OBJ)/raku_builtins.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/scan_builtins.c -o $(OBJ)/scan_builtins.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/pl_runtime.c  -o $(OBJ)/pl_runtime.o
	$(CC) $(CRT)   -c $(SRC)/runtime/interp/icon_box_rt.c  -o $(OBJ)/icon_box_rt.o
	$(CC) $(CRT)   -c $(SRC)/runtime/snobol4/coerce.c      -o $(OBJ)/coerce.o
	$(CC) $(CRT)   -c $(SRC)/lower/ast_clone.c    -o $(OBJ)/ast_clone.o
	$(CC) $(CRT)   -c $(SRC)/lower/scrip_ir.c     -o $(OBJ)/scrip_ir.o
	$(CC) $(CRT)   -c $(SRC)/lower/ir_exec.c      -o $(OBJ)/ir_exec.o
	$(CC) $(CRT)   -c $(SRC)/lower/sm_prog.c    -o $(OBJ)/sm_prog.o
	$(CC) $(CRT)   -c $(SRC)/processor/sm_interp.c  -o $(OBJ)/sm_interp.o
	$(CC) $(CRT)   -c $(SRC)/lower/lower.c      -o $(OBJ)/lower.o
	$(CC) $(CRT)   -c $(SRC)/lower/lower_ctx.c  -o $(OBJ)/lower_ctx.o
	$(CC) $(CRT)   -c $(SRC)/lower/lower_pat_dcg.c -o $(OBJ)/lower_pat_dcg.o
	$(CC) $(CRT)   -c $(SRC)/lower/lower_icn.c     -o $(OBJ)/lower_icn.o
	$(CC) $(CRT)   -c $(SRC)/lower/lower_pl.c      -o $(OBJ)/lower_pl.o
	$(CC) $(CRT)   -c $(SRC)/lower/lower_sno.c     -o $(OBJ)/lower_sno.o
	$(CC) $(CRT)   -c $(SRC)/processor/sm_image.c   -o $(OBJ)/sm_image.o
	$(CC) $(CRT)   -c $(SRC)/processor/sm_jit_interp.c -o $(OBJ)/sm_jit_interp.o
	$(CC) $(CRT)   -c $(SRC)/emitter/emit_sm.c -o $(OBJ)/emit_sm.o
	$(CC) $(CRT)   -c $(SRC)/emitter/emit_bb.c -o $(OBJ)/emit_bb.o
	$(CC) $(CRT)   -c $(SRC)/runtime/rt/rt.c   -o $(OBJ)/rt.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_globals.c -o $(OBJ)/interp_globals.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_label.c   -o $(OBJ)/interp_label.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_hooks.c   -o $(OBJ)/interp_hooks.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_data.c    -o $(OBJ)/interp_data.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_call.c    -o $(OBJ)/interp_call.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_ref.c     -o $(OBJ)/interp_ref.o
	$(CC) $(CRT)   -c $(SRC)/driver/interp_ast_stubs.c -o $(OBJ)/interp_ast_stubs.o
	$(CC) $(CRT)   -c $(SRC)/driver/scrip_sm.c       -o $(OBJ)/scrip_sm.o
	$(CC) $(CRT)   -c $(SRC)/driver/stmt_ast.c       -o $(OBJ)/stmt_ast.o
	$(CC) $(CRT)   -c $(SRC)/driver/sync_monitor.c -o $(OBJ)/sync_monitor.o
	$(CC) $(CRT)   -c $(SRC)/driver/polyglot.c -o $(OBJ)/polyglot.o
	$(CC) $(CRT)   -c $(SRC)/driver/scrip.c  -o $(OBJ)/scrip_driver.o
	$(CC) -m64 -no-pie $(OBJ)/*.o $(LIBS) -o scrip
	@echo "Built: scrip"

# backward-compat symlink
scrip-interp: scrip
	@ln -sf scrip scrip-interp

# ── test_emit_io — EC-UNI-11/12 self-test for Layer-3 string-builder primitives ──
# Standalone; no scrip dependency.  Runs in <100 ms.
test_emit_io:
	$(CC) -O0 -g -Wall -I$(SRC) -I$(SRC)/include -I$(SRC)/lower -I$(SRC)/processor -I$(SRC)/emitter -I$(SRC)/runtime/snobol4 -I$(SRC)/runtime \
	    $(SRC)/emitter/emit_io.c $(SRC)/emitter/emit_globals.c $(SRC)/emitter/test_emit_io.c -o /tmp/test_emit_io
	/tmp/test_emit_io
	@echo "OK  test_emit_io"

# ── scrip-monitor: scrip with CSNOBOL4 4th executor linked in (IM-15b) ───────
# Build: make scrip-monitor CSN_A=/home/claude/csnobol4/libcsnobol4.a
# Requires: bash scripts/build_csnobol4_archive.sh first
CSN_A   ?= /home/claude/csnobol4/libcsnobol4.a
CSN_INC ?= /home/claude/csnobol4

scrip-monitor:
	@# Build all scrip objects, then relink with CSNOBOL4 4th executor
	$(MAKE) -f Makefile scrip
	$(CC) $(CRT) -DWITH_CSNOBOL4=1 -I$(CSN_INC) \
	      -c $(SRC)/driver/csnobol4_shim.c -o $(OBJ)/csnobol4_shim_csn.o
	$(CC) $(CRT) -DWITH_CSNOBOL4=1 \
	      -c $(SRC)/driver/sync_monitor.c -o $(OBJ)/sync_monitor_csn.o
	$(CC) -m64 -no-pie \
	      $(OBJ)/csnobol4_shim_csn.o $(OBJ)/sync_monitor_csn.o \
	      $(filter-out $(OBJ)/sync_monitor.o $(OBJ)/sync_monitor_csn.o $(OBJ)/csnobol4_shim.o $(OBJ)/csnobol4_shim_csn.o $(OBJ)/scrip_driver.o, $(wildcard $(OBJ)/*.o)) \
	      $(OBJ)/scrip_driver.o \
	      $(CSN_A) $(LIBS) -lutil -ldl -lz -lbz2 -o scrip-monitor
	@echo "Built: scrip-monitor (with CSNOBOL4 4th executor)"

# ── monitor_ipc.so ────────────────────────────────────────────────────────────

monitor-ipc:
	gcc -shared -fPIC \
	    -o test/monitor/monitor_ipc.so \
	    test/monitor/monitor_ipc.c
	@echo "Built: test/monitor/monitor_ipc.so"

# ── Environment setup (idempotent) ────────────────────────────────────────────

setup:
	bash $(ROOT)/setup.sh

# ── Test targets ──────────────────────────────────────────────────────────────

test: scrip
	CORPUS=$(CORPUS) bash test/run_interp_broad.sh

test-ir: scrip
	INTERP="./scrip --ir-run" CORPUS=$(CORPUS) bash test/run_interp_broad.sh

test-all: test test-ir

# ── EM-9: jit-emit-test — smoke + em8 gate for --jit-emit --x64 ──────────────
# Runs: test_smoke_snobol4.sh (7/7) + test_gate_em8_snocone_jit_emit.sh (5/5).
# Requires: libscrip_rt (built into out/libscrip_rt.so).
jit-emit-test: scrip libscrip_rt
	@bash scripts/test_smoke_snobol4.sh
	@bash scripts/test_gate_em8_snocone_jit_emit.sh

.PHONY: jit-emit-test

# ── Runner wrappers ───────────────────────────────────────────────────────────

run: scrip
	./scrip $(SNO)

run-ir: scrip
	./scrip --ir-run $(SNO)

# Legacy JVM runner — uses old scrip text emitter until M-JITEM-JVM lands
run-jvm: scrip
	@mkdir -p $(JVM_CACHE); \
	base=$$(basename $(SNO) .sno); \
	hash=$$(echo $(SNO) | md5sum | cut -c1-8); \
	key=$${base}_$${hash}; \
	jfile=$(JVM_CACHE)/$${key}.j; \
	stamp=$(JVM_CACHE)/$${key}.stamp; \
	$(SCRIP_CC_BIN) -jvm $(SNO) > $$jfile; \
	classname=$$(grep '\.class' $$jfile | head -1 | awk '{print $$NF}'); \
	j_md5=$$(md5sum $$jfile | cut -d' ' -f1); \
	cached_md5=$$(cat $$stamp 2>/dev/null || echo ''); \
	if [ "$$j_md5" != "$$cached_md5" ] || [ ! -f $(JVM_CACHE)/$$classname.class ]; then \
	    java -jar $(JASMIN) $$jfile -d $(JVM_CACHE) >/dev/null; \
	    echo "$$j_md5" > $$stamp; \
	fi; \
	java -cp $(JVM_CACHE) $$classname

# Legacy .NET runner — uses old scrip text emitter until M-JITEM-NET lands
run-net: scrip
	@mkdir -p $(NET_CACHE); \
	base=$$(basename $(SNO) .sno); \
	hash=$$(echo $(SNO) | md5sum | cut -c1-8); \
	key=$${base}_$${hash}; \
	il=$(NET_CACHE)/$${key}.il; \
	exe=$(NET_CACHE)/$${key}.exe; \
	stamp=$(NET_CACHE)/$${key}.stamp; \
	$(SCRIP_CC_BIN) -net $(SNO) > $$il; \
	il_md5=$$(md5sum $$il | cut -d' ' -f1); \
	cached_md5=$$(cat $$stamp 2>/dev/null || echo ''); \
	if [ "$$il_md5" != "$$cached_md5" ] || [ ! -f $$exe ]; then \
	    ilasm $$il /output:$$exe >/dev/null; \
	    echo "$$il_md5" > $$stamp; \
	fi; \
	mono $$exe

# ── Clean ─────────────────────────────────────────────────────────────────────

clean:
	rm -rf $(OBJ) scrip scrip-interp

distclean: clean
	rm -rf $(JVM_CACHE) $(NET_CACHE) /tmp/snobol4_asm_* /tmp/scrip_cc_*
