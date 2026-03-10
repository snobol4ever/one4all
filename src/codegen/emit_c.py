"""
emit_c.py — SNOBOL4-tiny C-with-gotos code generator

Takes an IR Graph and emits a self-contained C file that:
  - declares all static storage in a .bss-equivalent (static globals)
  - emits each node as inlined alpha/beta/gamma/omega labels
  - wires gamma/omega according to the node's position in the pattern

Entry point: emit_program(graph, root_name, subject) -> str

The emitter resolves Ref nodes lazily: a Ref("X") emits gotos to
X_alpha / X_beta, which must be defined elsewhere in the same output.
"""

from ir import (Graph, Node, Lit, Any as IrAny, Span, Break,
                Len, Pos, Rpos, Arb, Arbno, Alt, Cat, Assign, Ref)


class Emitter:
    def __init__(self, graph: Graph):
        self.graph = graph
        self.lines: list[str] = []
        self.counter = 0
        self.statics: list[str] = []  # static variable declarations

    def fresh_id(self, prefix: str) -> str:
        self.counter += 1
        return f"{prefix}{self.counter}"

    def emit(self, line: str):
        self.lines.append(line)

    def static(self, decl: str):
        self.statics.append(decl)

    def emit_node(self, node: Node, nid: str,
                  gamma: str, omega: str):
        """Emit C labels for one IR node."""

        if isinstance(node, Lit):
            n = len(node.s.encode())
            safe = node.s.replace('\\', '\\\\').replace('"', '\\"')
            self.static(f"static int64_t {nid}_saved_cursor;")
            self.emit(f"{nid}_alpha:")
            self.emit(f"    if (cursor + {n} > subject_len) goto {omega};")
            self.emit(f'    if (memcmp(subject + cursor, "{safe}", {n}) != 0) goto {omega};')
            self.emit(f"    {nid}_saved_cursor = cursor;")
            self.emit(f"    cursor += {n};")
            self.emit(f"    goto {gamma};")
            self.emit(f"{nid}_beta:")
            self.emit(f"    cursor = {nid}_saved_cursor;")
            self.emit(f"    goto {omega};")

        elif isinstance(node, Pos):
            self.emit(f"{nid}_alpha:")
            self.emit(f"    if (cursor != {node.n}) goto {omega};")
            self.emit(f"    goto {gamma};")
            self.emit(f"{nid}_beta:")
            self.emit(f"    goto {omega};")

        elif isinstance(node, Rpos):
            self.emit(f"{nid}_alpha:")
            self.emit(f"    if (cursor != subject_len - {node.n}) goto {omega};")
            self.emit(f"    goto {gamma};")
            self.emit(f"{nid}_beta:")
            self.emit(f"    goto {omega};")

        elif isinstance(node, Len):
            self.static(f"static int64_t {nid}_saved_cursor;")
            self.emit(f"{nid}_alpha:")
            self.emit(f"    if (cursor + {node.n} > subject_len) goto {omega};")
            self.emit(f"    {nid}_saved_cursor = cursor;")
            self.emit(f"    cursor += {node.n};")
            self.emit(f"    goto {gamma};")
            self.emit(f"{nid}_beta:")
            self.emit(f"    cursor = {nid}_saved_cursor;")
            self.emit(f"    goto {omega};")

        elif isinstance(node, Span):
            safe_cs = node.charset.replace('\\', '\\\\').replace('"', '\\"')
            self.static(f"static int64_t {nid}_saved_cursor;")
            self.emit(f"{nid}_alpha:")
            self.emit(f"    {{")
            self.emit(f'        const char *cs = "{safe_cs}";')
            self.emit(f"        int64_t start = cursor;")
            self.emit(f"        while (cursor < subject_len && strchr(cs, subject[cursor])) cursor++;")
            self.emit(f"        if (cursor == start) goto {omega};")
            self.emit(f"        {nid}_saved_cursor = start;")
            self.emit(f"    }}")
            self.emit(f"    goto {gamma};")
            self.emit(f"{nid}_beta:")
            self.emit(f"    if (cursor <= {nid}_saved_cursor + 1) {{")
            self.emit(f"        cursor = {nid}_saved_cursor;")
            self.emit(f"        goto {omega};")
            self.emit(f"    }}")
            self.emit(f"    cursor--;")
            self.emit(f"    goto {gamma};")

        elif isinstance(node, Cat):
            # Inline both children; wire P_gamma -> Q_alpha, Q_omega -> P_beta
            left_id  = self.fresh_id("cat_l")
            right_id = self.fresh_id("cat_r")
            self.emit(f"{nid}_alpha: /* CAT — enter left */")
            self.emit(f"    goto {left_id}_alpha;")
            self.emit_node(node.left,  left_id,  gamma=f"{right_id}_alpha", omega=f"{nid}_beta")
            self.emit_node(node.right, right_id, gamma=gamma,               omega=f"{left_id}_beta")
            self.emit(f"{nid}_beta:")
            self.emit(f"    goto {omega};")

        elif isinstance(node, Alt):
            left_id  = self.fresh_id("alt_l")
            right_id = self.fresh_id("alt_r")
            self.emit(f"{nid}_alpha: /* ALT — try left */")
            self.emit(f"    goto {left_id}_alpha;")
            self.emit_node(node.left,  left_id,  gamma=gamma, omega=f"{right_id}_alpha")
            self.emit_node(node.right, right_id, gamma=gamma, omega=omega)
            self.emit(f"{nid}_beta:")
            self.emit(f"    goto {right_id}_beta;")

        elif isinstance(node, Assign):
            child_id = self.fresh_id("assign_c")
            var_safe = node.var.upper()
            self.static(f"static str_t var_{var_safe};")
            self.emit(f"{nid}_alpha:")
            self.static(f"static int64_t {nid}_start;")
            self.emit(f"    {nid}_start = cursor;")
            self.emit(f"    goto {child_id}_alpha;")
            self.emit_node(node.child, child_id,
                           gamma=f"{nid}_do_assign",
                           omega=omega)
            self.emit(f"{nid}_do_assign:")
            self.emit(f"    var_{var_safe}.ptr = subject + {nid}_start;")
            self.emit(f"    var_{var_safe}.len = cursor - {nid}_start;")
            if var_safe == "OUTPUT":
                self.emit(f"    sno_output(var_{var_safe});")
            self.emit(f"    goto {gamma};")
            self.emit(f"{nid}_beta:")
            self.emit(f"    goto {child_id}_beta;")

        elif isinstance(node, Arbno):
            # ARBNO(child): zero-or-more non-overlapping matches of child.
            # α: succeed immediately (zero iterations).
            # β: push cursor, try child_α fresh (one more iteration).
            # child_γ: child matched — succeed again (cursor advanced).
            # child_ω: child failed — restore cursor, propagate failure.
            child_id = self.fresh_id("arbno_c")
            self.static(f"static int64_t {nid}_cursors[64];")
            self.static(f"static int     {nid}_depth;")
            self.emit(f"{nid}_alpha:")
            self.emit(f"    {nid}_depth = -1;")
            self.emit(f"    goto {gamma};              /* ARBNO: zero matches → succeed */")
            self.emit(f"{nid}_beta:")
            self.emit(f"    {nid}_depth++;")
            self.emit(f"    if ({nid}_depth >= 64) goto {omega};  /* stack overflow */")
            self.emit(f"    {nid}_cursors[{nid}_depth] = cursor;")
            self.emit(f"    goto {child_id}_alpha;")
            self.emit_node(node.child, child_id,
                           gamma=f"{nid}_child_ok",
                           omega=f"{nid}_child_fail")
            self.emit(f"{nid}_child_ok:")
            self.emit(f"    goto {gamma};              /* child matched → ARBNO succeeds again */")
            self.emit(f"{nid}_child_fail:")
            self.emit(f"    cursor = {nid}_cursors[{nid}_depth];")
            self.emit(f"    {nid}_depth--;")
            self.emit(f"    goto {omega};              /* child failed → ARBNO fails */")

        elif isinstance(node, Ref):
            # Forward / backward reference to a named top-level pattern
            self.emit(f"{nid}_alpha:")
            self.emit(f"    goto {node.name}_alpha;")
            self.emit(f"{nid}_beta:")
            self.emit(f"    goto {node.name}_beta;")

        else:
            self.emit(f"/* TODO: {type(node).__name__} not yet implemented */")
            self.emit(f"{nid}_alpha: goto {omega};")
            self.emit(f"{nid}_beta:  goto {omega};")


def emit_program(graph: Graph, root_name: str,
                 subject: str = "", include_main: bool = True) -> str:
    """Emit a complete compilable C file for the given pattern graph."""
    em = Emitter(graph)

    # Emit all named top-level nodes
    for name in graph.names():
        node = graph.get(name)
        em.emit(f"\n/* ===== pattern: {name} ===== */")
        em.emit_node(node, nid=name,
                     gamma=f"{name}_MATCH_SUCCESS",
                     omega=f"{name}_MATCH_FAIL")
        em.emit(f"{name}_MATCH_SUCCESS: return 0; /* matched */")
        em.emit(f"{name}_MATCH_FAIL:    return 1; /* failed  */")

    # Assemble final C file
    out = []
    out.append("/* Generated by SNOBOL4-tiny emit_c.py — DO NOT EDIT */")
    out.append("#include <stdint.h>")
    out.append("#include <string.h>")
    out.append("#include <stdio.h>")
    out.append('#include "../../src/runtime/runtime.h"')
    out.append("")
    out.append("/* === static storage === */")
    for s in em.statics:
        out.append(s)
    out.append("")

    if include_main:
        safe_subj = subject.replace('\\', '\\\\').replace('"', '\\"')
        out.append("int main(void) {")
        out.append(f'    const char *subject    = "{safe_subj}";')
        out.append(f"    int64_t     subject_len = {len(subject.encode())};")
        out.append( "    int64_t     cursor      = 0;")
        out.append( "    (void)cursor; (void)subject; (void)subject_len;")
        out.append("")
        out.append(f"    goto {root_name}_alpha;")
        out.append("")

    for line in em.lines:
        out.append("    " + line if include_main else line)

    if include_main:
        out.append("}")

    return "\n".join(out) + "\n"


# ---------- smoke test -----------------------------------------------
if __name__ == "__main__":
    from ir import Graph, Cat, Pos, Rpos, Lit
    g = Graph()
    g.add("root", Cat(Pos(0), Cat(Lit("hello"), Rpos(0))))
    print(emit_program(g, "root", subject="hello"))
