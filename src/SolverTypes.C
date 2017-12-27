#include "SolverTypes.h"

Lit operator ~ (Lit p) { Lit q; q.x = p.x ^ 1; return q; }

bool sign  (Lit p) { return p.x & 1; }
int  var   (Lit p) { return p.x >> 1; }
int  index (Lit p) { return p.x; }                // A "toInt" method that guarantees small, positive integers suitable for array indexing.
Lit  toLit (int i) { Lit p; p.x = i; return p; }  // Inverse of 'index()'.
Lit  unsign(Lit p) { Lit q; q.x = p.x & ~1; return q; }
Lit  id    (Lit p, bool sgn) { Lit q; q.x = p.x ^ (int)sgn; return q; }

bool operator == (Lit p, Lit q) { return index(p) == index(q); }
bool operator <  (Lit p, Lit q) { return index(p)  < index(q); }  // '<' guarantees that p, ~p are adjacent in the ordering.


Clause* Clause_new(bool learnt, const vec<Lit>& ps) {
        assert(sizeof(Lit)      == sizeof(uint));
        assert(sizeof(float)    == sizeof(uint));
        void*   mem = xmalloc<char>(sizeof(Clause) + sizeof(uint)*(ps.size() + (int)learnt));
        return new (mem) Clause(learnt, ps); }

GClause GClause_new(Lit p)     { return GClause((void*)((index(p) << 1) + 1)); }
GClause GClause_new(Clause* c) { assert(((uintp)c & 1) == 0); return GClause((void*)c); }