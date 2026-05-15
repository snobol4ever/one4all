#include "prolog_atom.h"
#include "term.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gc.h>
int ATOM_DOT  = -1;
int ATOM_NIL  = -1;
int ATOM_TRUE = -1;
int ATOM_FAIL = -1;
int ATOM_CUT  = -1;
#define ATOM_INIT_CAP  256
static char  **atom_names = NULL;
static int     atom_len   = 0;
static int     atom_cap   = 0;
#define HT_INIT_SIZE  512
typedef struct { char *key; int id; } HEntry;
static HEntry *ht      = NULL;
static int     ht_size = 0;
static int     ht_used = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static unsigned int ht_hash(const char *s) {
    unsigned int h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void ht_grow(int new_size) {
    HEntry *old = ht;
    int     old_size = ht_size;
    ht = GC_malloc(new_size * sizeof(HEntry));
    memset(ht, 0, new_size * sizeof(HEntry));
    ht_size = new_size;
    ht_used = 0;
    for (int i = 0; i < old_size; i++) {
        if (!old[i].key) continue;
        unsigned int h = ht_hash(old[i].key) & (ht_size - 1);
        while (ht[h].key) h = (h + 1) & (ht_size - 1);
        ht[h] = old[i];
        ht_used++;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int prolog_atom_intern(const char *name) {
    if (!name) name = "";
    if (!ht) {
        ht_size = HT_INIT_SIZE;
        ht = GC_malloc(ht_size * sizeof(HEntry));
        memset(ht, 0, ht_size * sizeof(HEntry));
    }
    if (!atom_names) {
        atom_cap  = ATOM_INIT_CAP;
        atom_names = GC_malloc(atom_cap * sizeof(char *));
        memset(atom_names, 0, atom_cap * sizeof(char *));
    }
    unsigned int h = ht_hash(name) & (ht_size - 1);
    while (ht[h].key) {
        if (strcmp(ht[h].key, name) == 0) return ht[h].id;
        h = (h + 1) & (ht_size - 1);
    }
    if (ht_used * 2 >= ht_size) {
        ht_grow(ht_size * 2);
        h = ht_hash(name) & (ht_size - 1);
        while (ht[h].key) h = (h + 1) & (ht_size - 1);
    }
    if (atom_len >= atom_cap) {
        int old_cap = atom_cap;
        atom_cap *= 2;
        atom_names = GC_realloc(atom_names, atom_cap * sizeof(char *));
        memset(atom_names + old_cap, 0, (atom_cap - old_cap) * sizeof(char *));
    }
    char *copy = GC_strdup(name);
    int   id   = atom_len++;
    atom_names[id] = copy;
    ht[h].key = copy;
    ht[h].id  = id;
    ht_used++;
    return id;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *prolog_atom_name(int id) {
    if (id < 0 || id >= atom_len) return NULL;
    return atom_names[id];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int prolog_atom_count(void) { return atom_len; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void prolog_atom_init(void) {
    ATOM_DOT  = prolog_atom_intern(".");
    ATOM_NIL  = prolog_atom_intern("[]");
    ATOM_TRUE = prolog_atom_intern("true");
    ATOM_FAIL = prolog_atom_intern("fail");
    ATOM_CUT  = prolog_atom_intern("!");
}
#include <stddef.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Term *term_new_atom(int atom_id) {
    Term *t = GC_malloc(sizeof(Term));
    memset(t, 0, sizeof(Term));
    t->tag     = TERM_ATOM;
    t->atom_id = atom_id;
    return t;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Term *term_new_var(int var_slot) {
    Term *t = GC_malloc(sizeof(Term));
    memset(t, 0, sizeof(Term));
    t->tag        = TERM_VAR;
    t->var_slot   = var_slot;
    t->saved_slot = var_slot;
    return t;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Term *term_new_compound(int functor, int arity, Term **args) {
    Term *t = GC_malloc(sizeof(Term));
    memset(t, 0, sizeof(Term));
    t->tag              = TERM_COMPOUND;
    t->compound.functor = functor;
    t->compound.arity   = arity;
    if (arity > 0 && args) {
        t->compound.args = GC_malloc(arity * sizeof(Term *));
        memcpy(t->compound.args, args, arity * sizeof(Term *));
    } else {
        t->compound.args = NULL;
    }
    return t;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Term *term_new_int(long ival) {
    Term *t = GC_malloc(sizeof(Term));
    memset(t, 0, sizeof(Term));
    t->tag  = TERM_INT;
    t->ival = ival;
    return t;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Term *term_new_float(double fval) {
    Term *t = GC_malloc(sizeof(Term));
    memset(t, 0, sizeof(Term));
    t->tag  = TERM_FLOAT;
    t->fval = fval;
    return t;
}
