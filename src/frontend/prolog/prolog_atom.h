#ifndef PL_ATOM_H
#define PL_ATOM_H
/*
 * prolog_atom.h — atom interning table for the Prolog frontend
 *
 * All Prolog atoms (including functor names) are interned here.
 * prolog_atom_intern() returns a stable integer ID for any atom string.
 * prolog_atom_name()   returns the canonical string for an ID.
 *
 * IDs start at 0.  The table grows dynamically.
 * Thread-safety: not required (single-threaded compiler).
 */

/* Initialise the table and intern the well-known atoms.
 * Must be called once before any prolog_atom_intern() call.           */
void prolog_atom_init(void);

/* Intern a NUL-terminated string.  Returns its stable atom ID.
 * Duplicate calls with the same string return the same ID.         */
int prolog_atom_intern(const char *name);

/* Return the canonical atom string for id, or NULL if out of range. */
const char *prolog_atom_name(int id);

/* Total number of interned atoms so far.                            */
int prolog_atom_count(void);

#endif /* PL_ATOM_H */
