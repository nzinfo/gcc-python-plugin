#   Copyright 2012 David Malcolm <dmalcolm@redhat.com>
#   Copyright 2012 Red Hat, Inc.
#
#   This is free software: you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see
#   <http://www.gnu.org/licenses/>.

############################################################################
# Preprocessing phase
############################################################################
import gcc

from gccutils.graph.stmtgraph import SplitPhiNode
from gccutils.graph.supergraph import CallToReturnSiteEdge

from sm.solver import simplify, AbstractValue, fixed_point_solver

# For applying boolean not:
inverseops =  {'==' : '!=',
               '!=' : '==',
               '<'  : '>=',
               '<=' : '>',
               '>'  : '<=',
               '>=' : '<',
               }

# For flipping LHS and RHS:
flippedops =  {
    # Equality/inequality is symmetric:
    '==' : '==',
    '!=' : '!=',

    # Comparisons change direction:
    '<'  : '>',
    '<=' : '>=',
    '>'  : '<',
    '>=' : '<=',
    }

# Mapping from gcc expression codes to Python method names:
exprcodenames = {
    gcc.PlusExpr: '__add__',
    gcc.MultExpr: '__mul__',
    gcc.TruncDivExpr: '__div__',
    }

class Fact:
    def __init__(self, lhs, op, rhs):
        self.lhs = lhs
        self.op = op
        self.rhs = rhs

    def __str__(self):
        return '%s %s %s' % (self.lhs, self.op, self.rhs)

    def __repr__(self):
        return 'Fact(%r, %r, %r)' % (self.lhs, self.op, self.rhs)

    def __eq__(self, other):
        if isinstance(other, Fact):
            if self.lhs == other.lhs:
                if self.op == other.op:
                    if self.rhs == other.rhs:
                        return True

    def __hash__(self):
        return hash(self.lhs) ^ hash(self.op) ^ hash(self.rhs)

    def __lt__(self, other):
        # Support sorting of facts to allow for consistent ordering in
        # test output
        if isinstance(other, Fact):
            return (self.lhs, self.op, self.rhs) < (other.lhs, other.op, other.rhs)

class Factoid:
    """
    Like a fact, but has an context-dependent LHS
    """
    def __init__(self, op, rhs):
        self.op = op
        self.rhs = rhs

    def __str__(self):
        return '%s %s' % (self.op, self.rhs)

    def __repr__(self):
        return 'Factoid(%r, %r)' % (self.op, self.rhs)

    def __eq__(self, other):
        if isinstance(other, Factoid):
            if self.op == other.op:
                if self.rhs == other.rhs:
                    return True

    def __hash__(self):
        return hash(self.op) ^ hash(self.rhs)

    def __lt__(self, other):
        # Support sorting of facts to allow for consistent ordering in
        # test output
        if isinstance(other, Factoid):
            return (self.op, self.rhs) < (other.op, other.rhs)

    def apply_binary_op_to_constant(self, opname, other):
        const = self.rhs
        if isinstance(const, gcc.Constant):
            const = const.constant
        return Factoid(self.op, getattr(const, opname)(other))

class Facts(AbstractValue, set):
    def __init__(self, *args):
        set.__init__(self, *args)

        # lazily constructed
        # dict from expr to (shared) sets of exprs
        self.partitions = None

    def __str__(self):
        return '(%s)' % (' && '.join([str(fact)
                                      for fact in sorted(self)]))

    @classmethod
    def make_entry_point(cls, ctxt, node):
        return Facts()

    @classmethod
    def get_edge_value(cls, ctxt, srcvalue, edge):
        # Don't propagate information along the *intra*procedural edge
        # of an interprocedural callsite (i.e. one where both caller and
        # callee have their CFG in the supergraph), so that if the called
        # function never returns, we don't erroneously let that affect
        # subsequent state within the callee.
        # This means that e.g. within
        #     if (i > 10) {
        #        something_that_calls_abort();
        #     }
        #     foo()
        # that only facts from the false edge reach the call to foo(), and
        # hence we know there that (i <= 10)
        if isinstance(edge, CallToReturnSiteEdge):
            return None, None

        stmt = edge.srcnode.stmt
        dstfacts = Facts(srcvalue)
        if isinstance(stmt, gcc.GimpleAssign):
            exprcode = stmt.exprcode
            if 1:
                ctxt.debug('gcc.GimpleAssign: %s', stmt)
                ctxt.debug('  stmt.lhs: %r', stmt.lhs)
                ctxt.debug('  stmt.rhs: %r', stmt.rhs)
                ctxt.debug('  exprcode: %r', exprcode)
            if exprcode in exprcodenames:
                lhs = simplify(stmt.lhs)
                rhs0 = simplify(stmt.rhs[0])
                rhs1 = simplify(stmt.rhs[1])
                dstfacts._assignment_from_binary_op(ctxt, lhs, rhs0, rhs1,
                                                    exprcodenames[exprcode])
            elif exprcode in (gcc.IntegerCst,
                              gcc.ParmDecl, gcc.VarDecl,
                              gcc.MemRef, gcc.ComponentRef):
                assert len(stmt.rhs) == 1
                lhs = simplify(stmt.lhs)
                rhs = simplify(stmt.rhs[0])
                dstfacts._assignment(lhs, rhs)
            else:
                # We don't know how to handle this expression code, so
                # just forget what we knew about the LHS:
                lhs = simplify(stmt.lhs)
                dstfacts._remove_invalidated_facts(lhs)

        elif isinstance(stmt, gcc.GimpleCond):
            if 1:
                ctxt.debug('gcc.GimpleCond: %s', stmt)
            lhs = simplify(stmt.lhs)
            rhs = simplify(stmt.rhs)
            op = stmt.exprcode.get_symbol()
            if edge.true_value:
                dstfacts.add( Fact(lhs, op, rhs) )
            if edge.false_value:
                op = inverseops[op]
                dstfacts.add( Fact(lhs, op, rhs) )
        elif isinstance(stmt, gcc.GimpleSwitch):
            if 0:
                ctxt.debug('gcc.GimpleSwitch: %s', stmt)
                print(stmt)
            indexvar = simplify(stmt.indexvar)

            # More than one gcc.CaseLabelExpr may point at the same label
            # These will be the same SupergraphEdge within the Supergraph
            # Hence a SupergraphEdge may have zero or more gcc.CaseLabelExpr

            minvalue = None
            maxvalue = None
            for cle in edge.stmtedge.caselabelexprs:
                if cle.low is not None:
                    if minvalue is None or minvalue > cle.low:
                       minvalue = cle.low

                    if cle.high is not None:
                        # a range from cle.low ... cle.high
                        if maxvalue is None or maxvalue < cle.high:
                            maxvalue = cle.high
                    else:
                        # a single value: cle.low
                        if maxvalue is None or maxvalue < cle.low:
                            maxvalue = cle.low
                if 0:
                    print('minvalue: %r' % minvalue)
                    print('maxvalue: %r' % maxvalue)
            if minvalue is not None:
                if minvalue == maxvalue:
                    dstfacts.add(Fact(indexvar, '==', minvalue))
                else:
                    dstfacts.add(Fact(indexvar, '>=', minvalue))
                    dstfacts.add(Fact(indexvar, '<=', maxvalue))
        elif isinstance(stmt, gcc.GimplePhi):
            srcnode = edge.srcnode
            if 1:
                ctxt.debug('gcc.GimplePhi: %s', stmt)
                ctxt.debug('  srcnode: %s', srcnode)
                ctxt.debug('  srcnode: %r', srcnode)
                ctxt.debug('  srcnode.innernode: %s', srcnode.innernode)
                ctxt.debug('  srcnode.innernode: %r', srcnode.innernode)
            assert isinstance(srcnode.supergraphnode.innernode, SplitPhiNode)
            rhs = simplify(srcnode.supergraphnode.innernode.rhs)
            lhs = simplify(srcnode.stmt.lhs)
            dstfacts._assignment(lhs, rhs)

        return dstfacts, None



    @classmethod
    def meet(cls, ctxt, lhs, rhs):
        # The set of valid known facts from multiple inedges is the
        # intersection of the facts from each inedge:
        if lhs is None:
            return rhs
        if rhs is None:
            return lhs
        return Facts(lhs & rhs)

    def _make_equiv_classes(self):
        partitions = {}

        for fact in self:
            lhs, op, rhs = fact.lhs, fact.op, fact.rhs
            if op == '==':
                if lhs in partitions:
                    if rhs in partitions:
                        merged = partitions[lhs] | partitions[rhs]
                    else:
                        partitions[lhs].add(rhs)
                        merged = partitions[lhs]
                else:
                    if rhs in partitions:
                        partitions[rhs].add(lhs)
                        merged = partitions[rhs]
                    else:
                        merged = set([lhs, rhs])
                partitions[lhs] = partitions[rhs] = merged

        self.partitions = partitions

    def get_equiv_classes(self):
        # Get equiv classes as a frozenset of frozensets:
        if self.partitions is None:
            self._make_equiv_classes()
        return frozenset([frozenset(equivcls)
                          for equivcls in self.partitions.values()])

    def is_possible(self, ctxt):
        ctxt.debug('is_possible: %s', self)
        # Work-in-progress implementation:
        # Gather vars by equivalence classes:

        if self.partitions is None:
            self._make_equiv_classes()
        ctxt.debug('partitions: %s', self.partitions)

        # There must be at most one specific constant within any equivalence
        # class:
        constants = {}
        for key in self.partitions:
            equivcls = self.partitions[key]
            for expr in equivcls:
                # (we support "int" here to make it easier to unit-test this code)
                if isinstance(expr, (gcc.IntegerCst, int)):
                    if key in constants:
                        # More than one (non-equal) constant within the class:
                        ctxt.debug('impossible: equivalence class for %s'
                                   ' contains non-equal constants %s and %s'
                                   % (equivcls, constants[key], expr))
                        return False
                    constants[key] = expr

        ctxt.debug('constants: %s' % constants)

        # Check any such constants against other inequalities:
        for fact in self:
            lhs, op, rhs = fact.lhs, fact.op, fact.rhs
            if op in ('!=', '<', '>'):
                if isinstance(rhs, (gcc.IntegerCst, int)):
                    if lhs in constants:
                        if constants[lhs] == rhs:
                            # a == CONST_1 && a != CONST_1 is impossible:
                            ctxt.debug('impossible: equivalence class for %s'
                                       ' equals constant %s but has %s %s %s',
                                       equivcls, constants[lhs], lhs, op, rhs)
                            return False

        # All tests passed:
        return True

    def get_aliases(self, expr):
        if self.partitions is None:
            self._make_equiv_classes()
        if expr in self.partitions:
            return frozenset(self.partitions[expr])
        else:
            return frozenset([expr])

    def expr_is_referenced_externally(self, ctxt, var):
        ctxt.debug('expr_is_referenced_externally(%s, %s)', self, var)
        for fact in self:
            lhs, op, rhs = fact.lhs, fact.op, fact.rhs
            if op == '==':
                # For now, any equality will do it
                # FIXME: needs to be something that isn't a local
                if var == lhs:
                    return True
                if var == rhs:
                    return True
        return False

    def _remove_invalidated_facts(self, expr):
        # remove any facts relating to an expression that might have changed
        # value:
        for fact in list(self):
            if expr == fact.lhs or expr == fact.rhs:
                self.remove(fact)

    def _assignment(self, lhs, rhs):
        if lhs == rhs:
            return
        self._remove_invalidated_facts(lhs)
        self.add( Fact(lhs, '==', rhs) )

    def _assignment_from_binary_op(self, ctxt, lhs, rhs0, rhs1, opname):
        if 1:
            ctxt.debug('_assignment_from_binary_op(%s, ...,'
                       ' lhs=%s, rhs0=%s, rhs1=%s, opname=%s)',
                       self, lhs, rhs0, rhs1, opname)

        rhs0factoids = list(self.iter_factoids_about(rhs0))
        ctxt.debug('rhs0factoids: %s', rhs0factoids)

        rhs1factoids = list(self.iter_factoids_about(rhs1))
        ctxt.debug('rhs1factoids: %s', rhs1factoids)

        # If we have, say, (i > 42) and we have i = i + 1
        # we want to end up with (i > 43):
        if isinstance(rhs1, gcc.IntegerCst):
            resultfactoids = Factoids([factoid.apply_binary_op_to_constant(opname, int(rhs1))
                                       for factoid in self.iter_factoids_about(rhs0)
                                       if isinstance(factoid.rhs, (gcc.Constant, int, long))])
            ctxt.debug('resultfactoids: %s', resultfactoids)

            self._remove_invalidated_facts(lhs)

            for fact in resultfactoids.make_facts_for_lhs(lhs):
                self.add(fact)
        else:
            self._remove_invalidated_facts(lhs)

    def iter_factoids_about(self, expr):
        for fact in self:
            if expr == fact.lhs:
                yield Factoid(fact.op, fact.rhs)
            if expr == fact.rhs:
                yield Factoid(flippedops[fact.op], fact.rhs)

class Factoids(set):
    def __str__(self):
        return '(%s)' % (' && '.join([str(factoid)
                                      for factoid in sorted(self)]))

    def make_facts_for_lhs(self, lhs):
        return Facts([Fact(lhs, factoid.op, factoid.rhs)
                      for factoid in self])

def remove_impossible(ctxt, facts_for_node, graph):
    # Purge graph of any nodes with contradictory facts which are thus
    # impossible to actually reach
    from sm.solver import Timer
    with Timer(ctxt, 'remove_impossible'):
        changes = 0
        for node in list(graph.nodes):
            facts = facts_for_node[node]
            if facts is None or not facts.is_possible(ctxt):
                ctxt.log('removing impossible node: %s' % node)
                changes += graph.remove_node(node)
        ctxt.log('removed %i node(s)' % changes)
        return changes

def equivcls_to_str(equivcls):
    if equivcls is None:
        return 'None'
    return '{%s}' % ', '.join([str(expr)
                               for expr in equivcls])
