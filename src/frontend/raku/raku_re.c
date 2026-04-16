/*============================================================= raku_re.c ====
 * Raku regex engine — table-driven NFA (Thompson construction)
 *
 * PHASE 1  (RK-32): compile /pattern/ → Nfa_state[] table; report state count.
 * PHASE 2  (RK-33): table-driven NFA simulation for --ir-run  (raku_nfa_match).
 * PHASE 3  (RK-34): capture tracking (raku_nfa_exec) — $0/$1 positional groups.
 * PHASE 4  (future): BB lifter — NK_CAP_OPEN/CLOSE → BB capture boxes for
 *                    --sm-run/--jit-run and m:g/pat/ generator (RK-37).
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 *==========================================================================*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "raku_re.h"

/*------------------------------------------------------------------------
 * Character-class bitmap
 *----------------------------------------------------------------------*/
static void cc_set(Raku_cc *cc, unsigned char c) { cc->bits[c>>3] |= (1u << (c&7)); }
static void cc_setrange(Raku_cc *cc, unsigned char lo, unsigned char hi) {
    for (unsigned c = lo; c <= hi; c++) cc_set(cc, (unsigned char)c);
}
static void cc_invert(Raku_cc *cc) { for (int i=0;i<32;i++) cc->bits[i]^=0xFFu; }
int raku_cc_test(const Raku_cc *cc, unsigned char c) { return (cc->bits[c>>3]>>(c&7))&1; }

static void cc_fill_digit(Raku_cc *cc) { cc_setrange(cc,'0','9'); }
static void cc_fill_word(Raku_cc *cc)  { cc_setrange(cc,'a','z'); cc_setrange(cc,'A','Z');
                                          cc_setrange(cc,'0','9'); cc_set(cc,'_'); }
static void cc_fill_space(Raku_cc *cc) { cc_set(cc,' '); cc_set(cc,'\t'); cc_set(cc,'\n');
                                          cc_set(cc,'\r'); cc_set(cc,'\f'); cc_set(cc,'\v'); }

/*========================================================================
 * NFA state array (growable)
 *======================================================================*/
#define NFA_INIT_CAP 64

struct Raku_nfa {
    Nfa_state *states;
    int        n;        /* used */
    int        cap;
    int        start;
    int        accept;
    int        ngroups;  /* number of capture groups */
};

static int nfa_alloc(Raku_nfa *nfa) {
    if (nfa->n >= nfa->cap) {
        nfa->cap *= 2;
        nfa->states = realloc(nfa->states, (size_t)nfa->cap * sizeof(Nfa_state));
    }
    int id = nfa->n++;
    memset(&nfa->states[id], 0, sizeof(Nfa_state));
    nfa->states[id].id      = id;
    nfa->states[id].out1    = NFA_NULL;
    nfa->states[id].out2    = NFA_NULL;
    nfa->states[id].cap_idx = -1;
    nfa->states[id].kind    = NK_EPS;
    return id;
}

static int nfa_state(Raku_nfa *nfa, Nfa_kind kind, int out1, int out2) {
    int id = nfa_alloc(nfa);
    nfa->states[id].kind = kind;
    nfa->states[id].out1 = out1;
    nfa->states[id].out2 = out2;
    return id;
}

/*========================================================================
 * Parser
 *======================================================================*/
typedef struct {
    const char *pat;
    int         pos;
    int         len;
    Raku_nfa   *nfa;
    char        errbuf[128];
    int         ok;
    int         group_counter;  /* increments for each ( encountered */
} Re_parser;

static char peek(Re_parser *p)    { return p->pos < p->len ? p->pat[p->pos] : '\0'; }
static char consume(Re_parser *p) { return p->pos < p->len ? p->pat[p->pos++] : '\0'; }
static int  at_end(Re_parser *p)  { return p->pos >= p->len; }
static void re_err(Re_parser *p, const char *msg) {
    if (p->ok) { snprintf(p->errbuf, sizeof p->errbuf, "%s", msg); p->ok = 0; }
}

/* forward declarations */
static int parse_alt(Re_parser *p, int *out_start, int *out_accept);
static int parse_concat(Re_parser *p, int *out_start, int *out_accept);
static int parse_quantified(Re_parser *p, int *out_start, int *out_accept);
static int parse_atom(Re_parser *p, int *out_start, int *out_accept);

/*------------------------------------------------------------------------
 * parse_charclass — parse [...] (cursor past opening '[')
 *----------------------------------------------------------------------*/
static int parse_charclass(Re_parser *p) {
    int id = nfa_alloc(p->nfa);
    Nfa_state *s = &p->nfa->states[id];
    s->kind = NK_CLASS;
    s->out1 = NFA_NULL; s->out2 = NFA_NULL;
    int negate = 0, first = 1;
    if (peek(p) == '^') { negate = 1; consume(p); }
    while (!at_end(p)) {
        char c = peek(p);
        if (c == ']' && !first) { consume(p); break; }
        first = 0; consume(p);
        if (c == '\\') {
            if (at_end(p)) { re_err(p,"truncated escape in []"); return id; }
            char esc = consume(p);
            switch (esc) {
                case 'd': cc_fill_digit(&s->cc); break;
                case 'D': { Raku_cc t={0}; cc_fill_digit(&t); cc_invert(&t);
                             for(int i=0;i<32;i++) s->cc.bits[i]|=t.bits[i]; } break;
                case 'w': cc_fill_word(&s->cc); break;
                case 'W': { Raku_cc t={0}; cc_fill_word(&t); cc_invert(&t);
                             for(int i=0;i<32;i++) s->cc.bits[i]|=t.bits[i]; } break;
                case 's': cc_fill_space(&s->cc); break;
                case 'S': { Raku_cc t={0}; cc_fill_space(&t); cc_invert(&t);
                             for(int i=0;i<32;i++) s->cc.bits[i]|=t.bits[i]; } break;
                default:  cc_set(&s->cc,(unsigned char)esc); break;
            }
        } else {
            if (peek(p)=='-' && p->pos+1<p->len && p->pat[p->pos+1]!=']') {
                consume(p);
                char hi = consume(p);
                if ((unsigned char)c <= (unsigned char)hi)
                    cc_setrange(&s->cc,(unsigned char)c,(unsigned char)hi);
                else re_err(p,"invalid range");
            } else { cc_set(&s->cc,(unsigned char)c); }
        }
    }
    if (negate) cc_invert(&s->cc);
    return id;
}

/*------------------------------------------------------------------------
 * parse_atom
 *----------------------------------------------------------------------*/
static int parse_atom(Re_parser *p, int *out_start, int *out_accept) {
    if (at_end(p)) { re_err(p,"unexpected end of pattern"); return 0; }
    char c = peek(p);

    if (c == '(') {
        consume(p);
        /* assign a group index and emit CAP_OPEN */
        int gidx = p->group_counter++;
        if (gidx >= MAX_GROUPS) { re_err(p,"too many capture groups"); return 0; }
        int cap_open = nfa_alloc(p->nfa);
        p->nfa->states[cap_open].kind    = NK_CAP_OPEN;
        p->nfa->states[cap_open].cap_idx = gidx;
        p->nfa->states[cap_open].out1    = NFA_NULL;
        p->nfa->states[cap_open].out2    = NFA_NULL;

        int inner_start, inner_acc;
        if (!parse_alt(p, &inner_start, &inner_acc)) return 0;
        if (peek(p) != ')') { re_err(p,"missing ')'"); return 0; }
        consume(p);

        /* emit CAP_CLOSE */
        int cap_close = nfa_alloc(p->nfa);
        p->nfa->states[cap_close].kind    = NK_CAP_CLOSE;
        p->nfa->states[cap_close].cap_idx = gidx;
        p->nfa->states[cap_close].out1    = NFA_NULL;
        p->nfa->states[cap_close].out2    = NFA_NULL;

        /* chain: cap_open → inner_start; inner_acc → cap_close */
        p->nfa->states[cap_open].out1  = inner_start;
        p->nfa->states[inner_acc].out1 = cap_close;

        *out_start  = cap_open;
        *out_accept = cap_close;
        /* update ngroups */
        if (gidx + 1 > p->nfa->ngroups) p->nfa->ngroups = gidx + 1;
        return 1;
    }
    if (c == '[') { consume(p); int id=parse_charclass(p); if(!p->ok)return 0;
                    *out_start=*out_accept=id; return 1; }
    if (c == '^') { consume(p); int id=nfa_state(p->nfa,NK_ANCHOR_BOL,NFA_NULL,NFA_NULL);
                    *out_start=*out_accept=id; return 1; }
    if (c == '$') { consume(p); int id=nfa_state(p->nfa,NK_ANCHOR_EOL,NFA_NULL,NFA_NULL);
                    *out_start=*out_accept=id; return 1; }
    if (c == '.') { consume(p); int id=nfa_state(p->nfa,NK_ANY,NFA_NULL,NFA_NULL);
                    *out_start=*out_accept=id; return 1; }
    if (c == '\\') {
        consume(p);
        if (at_end(p)) { re_err(p,"truncated escape"); return 0; }
        char esc = consume(p);
        int id = nfa_alloc(p->nfa);
        Nfa_state *s = &p->nfa->states[id];
        s->out1=NFA_NULL; s->out2=NFA_NULL;
        switch (esc) {
            case 'd': s->kind=NK_CLASS; cc_fill_digit(&s->cc);  break;
            case 'D': s->kind=NK_CLASS; cc_fill_digit(&s->cc); cc_invert(&s->cc); break;
            case 'w': s->kind=NK_CLASS; cc_fill_word(&s->cc);   break;
            case 'W': s->kind=NK_CLASS; cc_fill_word(&s->cc);  cc_invert(&s->cc); break;
            case 's': s->kind=NK_CLASS; cc_fill_space(&s->cc);  break;
            case 'S': s->kind=NK_CLASS; cc_fill_space(&s->cc); cc_invert(&s->cc); break;
            default:  s->kind=NK_CHAR; s->ch=(unsigned char)esc; break;
        }
        *out_start=*out_accept=id; return 1;
    }
    if (c==')' || c=='|' || c=='*' || c=='+' || c=='?') {
        re_err(p,"unexpected meta"); return 0;
    }
    consume(p);
    int id=nfa_state(p->nfa,NK_CHAR,NFA_NULL,NFA_NULL);
    p->nfa->states[id].ch=(unsigned char)c;
    *out_start=*out_accept=id; return 1;
}

/*------------------------------------------------------------------------
 * parse_quantified
 *----------------------------------------------------------------------*/
static int parse_quantified(Re_parser *p, int *out_start, int *out_accept) {
    int a_start, a_acc;
    if (!parse_atom(p,&a_start,&a_acc)) return 0;
    char q = peek(p);
    if (q=='*'||q=='+'||q=='?') {
        consume(p);
        Raku_nfa *nfa=p->nfa;
        if (q=='*') {
            int split=nfa_alloc(nfa), acc=nfa_alloc(nfa);
            nfa->states[split].kind=NK_SPLIT; nfa->states[split].out1=a_start; nfa->states[split].out2=acc;
            nfa->states[a_acc].out1=split;
            nfa->states[acc].kind=NK_EPS; nfa->states[acc].out1=NFA_NULL; nfa->states[acc].out2=NFA_NULL;
            *out_start=split; *out_accept=acc;
        } else if (q=='+') {
            int split=nfa_alloc(nfa), acc=nfa_alloc(nfa);
            nfa->states[split].kind=NK_SPLIT; nfa->states[split].out1=a_start; nfa->states[split].out2=acc;
            nfa->states[a_acc].out1=split;
            nfa->states[acc].kind=NK_EPS; nfa->states[acc].out1=NFA_NULL; nfa->states[acc].out2=NFA_NULL;
            *out_start=a_start; *out_accept=acc;
        } else {
            int split=nfa_alloc(nfa), acc=nfa_alloc(nfa);
            nfa->states[split].kind=NK_SPLIT; nfa->states[split].out1=a_start; nfa->states[split].out2=acc;
            nfa->states[a_acc].out1=acc;
            nfa->states[acc].kind=NK_EPS; nfa->states[acc].out1=NFA_NULL; nfa->states[acc].out2=NFA_NULL;
            *out_start=split; *out_accept=acc;
        }
    } else { *out_start=a_start; *out_accept=a_acc; }
    return 1;
}

/*------------------------------------------------------------------------
 * parse_concat / parse_alt
 *----------------------------------------------------------------------*/
static int parse_concat(Re_parser *p, int *out_start, int *out_accept) {
    int started=0, c_start=NFA_NULL, c_acc=NFA_NULL;
    while (!at_end(p) && peek(p)!='|' && peek(p)!=')') {
        int q_start, q_acc;
        if (!parse_quantified(p,&q_start,&q_acc)) return 0;
        if (!started) { c_start=q_start; c_acc=q_acc; started=1; }
        else { p->nfa->states[c_acc].out1=q_start; c_acc=q_acc; }
    }
    if (!started) { int id=nfa_state(p->nfa,NK_EPS,NFA_NULL,NFA_NULL); c_start=c_acc=id; }
    *out_start=c_start; *out_accept=c_acc; return 1;
}

static int parse_alt(Re_parser *p, int *out_start, int *out_accept) {
    int l_start, l_acc;
    if (!parse_concat(p,&l_start,&l_acc)) return 0;
    while (peek(p)=='|') {
        consume(p);
        int r_start, r_acc;
        if (!parse_concat(p,&r_start,&r_acc)) return 0;
        Raku_nfa *nfa=p->nfa;
        int split=nfa_alloc(nfa), join=nfa_alloc(nfa);
        nfa->states[split].kind=NK_SPLIT; nfa->states[split].out1=l_start; nfa->states[split].out2=r_start;
        nfa->states[l_acc].out1=join; nfa->states[r_acc].out1=join;
        nfa->states[join].kind=NK_EPS; nfa->states[join].out1=NFA_NULL; nfa->states[join].out2=NFA_NULL;
        l_start=split; l_acc=join;
    }
    *out_start=l_start; *out_accept=l_acc; return 1;
}

/*========================================================================
 * Public: raku_nfa_build
 *======================================================================*/
Raku_nfa *raku_nfa_build(const char *pattern) {
    Raku_nfa *nfa = malloc(sizeof *nfa);
    nfa->cap=NFA_INIT_CAP; nfa->n=0; nfa->ngroups=0;
    nfa->states=malloc((size_t)nfa->cap*sizeof(Nfa_state));
    nfa->start=NFA_NULL; nfa->accept=NFA_NULL;

    Re_parser p;
    p.pat=pattern; p.pos=0; p.len=(int)strlen(pattern);
    p.nfa=nfa; p.ok=1; p.errbuf[0]='\0'; p.group_counter=0;

    int frag_start, frag_acc;
    if (!parse_alt(&p,&frag_start,&frag_acc)||!p.ok) {
        fprintf(stderr,"raku_re: compile error: %s\n",p.errbuf);
        raku_nfa_free(nfa); return NULL;
    }
    int acc=nfa_state(nfa,NK_ACCEPT,NFA_NULL,NFA_NULL);
    nfa->states[frag_acc].out1=acc;
    nfa->start=frag_start; nfa->accept=acc;
    return nfa;
}

int        raku_nfa_state_count(const Raku_nfa *nfa) { return nfa?nfa->n:0; }
int        raku_nfa_ngroups(const Raku_nfa *nfa)      { return nfa?nfa->ngroups:0; }
Nfa_state *raku_nfa_states(Raku_nfa *nfa)             { return nfa?nfa->states:NULL; }

void raku_nfa_free(Raku_nfa *nfa) { if(!nfa)return; free(nfa->states); free(nfa); }

/*========================================================================
 * Simulation — shared epsilon-closure walker
 * Handles: NK_EPS, NK_SPLIT, NK_ANCHOR_BOL/EOL, NK_CAP_OPEN/CLOSE
 * (CAP nodes are zero-width; they update captures[] then follow out1)
 *======================================================================*/
#define MAX_STATES 512

typedef struct { int ids[MAX_STATES]; int n; } State_set;

/* Per-state capture snapshot: what groups are open/closed at this active state */
typedef struct {
    int gs[MAX_GROUPS];   /* group start positions, -1 = unset */
    int ge[MAX_GROUPS];   /* group end positions,   -1 = unset */
} Cap_snap;

static Cap_snap g_snaps[MAX_STATES];  /* parallel to State_set */

static void ss_add(State_set *ss, Cap_snap *snaps, const Raku_nfa *nfa, int id,
                   char *visited, int pos, int slen, const Cap_snap *cur_snap) {
    if (id==NFA_NULL||visited[id]) return;
    visited[id]=1;
    Nfa_state *s=&nfa->states[id];
    switch (s->kind) {
        case NK_EPS:
            ss_add(ss,snaps,nfa,s->out1,visited,pos,slen,cur_snap); break;
        case NK_SPLIT:
            ss_add(ss,snaps,nfa,s->out1,visited,pos,slen,cur_snap);
            ss_add(ss,snaps,nfa,s->out2,visited,pos,slen,cur_snap); break;
        case NK_ANCHOR_BOL:
            if (pos==0) ss_add(ss,snaps,nfa,s->out1,visited,pos,slen,cur_snap); break;
        case NK_ANCHOR_EOL:
            if (pos==slen) ss_add(ss,snaps,nfa,s->out1,visited,pos,slen,cur_snap); break;
        case NK_CAP_OPEN: {
            Cap_snap ns=*cur_snap;
            ns.gs[s->cap_idx]=pos; ns.ge[s->cap_idx]=-1;
            ss_add(ss,snaps,nfa,s->out1,visited,pos,slen,&ns); break; }
        case NK_CAP_CLOSE: {
            Cap_snap ns=*cur_snap;
            ns.ge[s->cap_idx]=pos;
            ss_add(ss,snaps,nfa,s->out1,visited,pos,slen,&ns); break; }
        default:  /* consuming state: add to active set */
            snaps[ss->n]=*cur_snap;
            ss->ids[ss->n++]=id; break;
    }
}

static void eps_closure_into(State_set *ss, Cap_snap *snaps, const Raku_nfa *nfa,
                              int start, int pos, int slen, const Cap_snap *snap) {
    char visited[MAX_STATES]; memset(visited,0,(size_t)nfa->n);
    ss_add(ss,snaps,nfa,start,visited,pos,slen,snap);
}

/*------------------------------------------------------------------------
 * raku_nfa_match — fast path, no capture tracking
 *----------------------------------------------------------------------*/
int raku_nfa_match(const Raku_nfa *nfa, const char *subject) {
    Raku_match r; raku_nfa_exec(nfa,subject,&r); return r.matched;
}

/*------------------------------------------------------------------------
 * raku_nfa_exec — full execution with capture tracking
 *----------------------------------------------------------------------*/
void raku_nfa_exec(const Raku_nfa *nfa, const char *subject, Raku_match *result) {
    memset(result,0,sizeof *result);
    result->matched=0;
    for (int i=0;i<MAX_GROUPS;i++) { result->group_start[i]=-1; result->group_end[i]=-1; }
    result->ngroups=nfa->ngroups;

    if (!nfa||!subject) return;
    if (nfa->n>MAX_STATES) { fprintf(stderr,"raku_re: NFA too large\n"); return; }

    int slen=(int)strlen(subject);
    int anchored_bol=(nfa->states[nfa->start].kind==NK_ANCHOR_BOL);

    Cap_snap blank; for(int i=0;i<MAX_GROUPS;i++){blank.gs[i]=-1;blank.ge[i]=-1;}
    static Cap_snap cur_snaps[MAX_STATES], nxt_snaps[MAX_STATES];

    for (int start_pos=0; start_pos<=slen; start_pos++) {
        State_set cur; cur.n=0;
        eps_closure_into(&cur,cur_snaps,nfa,nfa->start,start_pos,slen,&blank);
        int pos=start_pos;

        int best_end = -1;
        Cap_snap best_snap; memset(&best_snap,0xff,sizeof best_snap);
        while (1) {
            /* record accept but keep driving for longer match */
            for (int i=0;i<cur.n;i++) {
                if (nfa->states[cur.ids[i]].kind==NK_ACCEPT && pos>=best_end) {
                    best_end  = pos;
                    best_snap = cur_snaps[i];
                }
            }
            if (pos>=slen) break;

            unsigned char ch=(unsigned char)subject[pos];
            State_set nxt; nxt.n=0;

            for (int i=0;i<cur.n;i++) {
                Nfa_state *s=&nfa->states[cur.ids[i]];
                int advance=0;
                switch (s->kind) {
                    case NK_CHAR:  advance=(s->ch==ch); break;
                    case NK_ANY:   advance=(ch!='\n');  break;
                    case NK_CLASS: advance=raku_cc_test(&s->cc,ch); break;
                    default: break;
                }
                if (advance) {
                    char visited[MAX_STATES]; memset(visited,0,(size_t)nfa->n);
                    ss_add(&nxt,nxt_snaps,nfa,s->out1,visited,pos+1,slen,&cur_snaps[i]);
                }
            }
            cur=nxt;
            /* copy nxt_snaps into cur_snaps */
            memcpy(cur_snaps,nxt_snaps,cur.n*sizeof(Cap_snap));
            pos++;
            if (cur.n==0) break;
        }
        /* commit best match for this start_pos */
        if (best_end >= 0) {
            result->matched    = 1;
            result->full_start = start_pos;
            result->full_end   = best_end;
            for (int g=0;g<nfa->ngroups;g++) {
                result->group_start[g] = best_snap.gs[g];
                result->group_end[g]   = best_snap.ge[g];
            }
            return;
        }
        if (anchored_bol) break;
    }
}
