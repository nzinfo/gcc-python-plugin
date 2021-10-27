#   Copyright 2011, 2012 David Malcolm <dmalcolm@redhat.com>
#   Copyright 2011, 2012 Red Hat, Inc.
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

import gcc
from gccutils import check_isinstance
from legacy_code.libcpychecker.types import register_type_object

# Recorded attribute data, primed with some special-case knowledge about
# the code that Cython and SWIG generate:
fnnames_returning_borrowed_refs = set([
        '__Pyx_GetStdout',

        'SWIG_Python_ErrorType',
        # returns a borrowed ref to one of the global exception objects

])

fnnames_setting_exception = set()
fnnames_setting_exception_on_negative_result = set()

# A dictionary mapping from fnname to set of argument indices:
stolen_refs_by_fnname = {}

def register_our_attributes():
    # Callback, called by the gcc.PLUGIN_ATTRIBUTES event

    # Handler for __attribute__((cpychecker_returns_borrowed_ref))
    # and #ifdef WITH_CPYCHECKER_RETURNS_BORROWED_REF_ATTRIBUTE
    def attribute_callback_for_returns_borrowed_ref(*args):
        if 0:
            print('attribute_callback_for_returns_borrowed_ref(%r)' % args)
        check_isinstance(args[0], gcc.FunctionDecl)
        fnname = args[0].name
        fnnames_returning_borrowed_refs.add(fnname)

    gcc.register_attribute('cpychecker_returns_borrowed_ref',
                           0, 0,
                           False, False, False,
                           attribute_callback_for_returns_borrowed_ref)
    gcc.define_macro('WITH_CPYCHECKER_RETURNS_BORROWED_REF_ATTRIBUTE')

    # Handler for __attribute__((cpychecker_steals_reference_to_arg(n)))
    # and #ifdef WITH_CPYCHECKER_STEALS_REFERENCE_TO_ARG_ATTRIBUTE
    def attribute_callback_for_steals_reference_to_arg(*args):
        if 0:
            print('attribute_callback_for_steals_reference_to_arg(%r)' % (args, ))
        check_isinstance(args[0], gcc.FunctionDecl)
        check_isinstance(args[1], gcc.IntegerCst)
        fnname = args[0].name
        argindex = int(args[1].constant)

        if fnname in stolen_refs_by_fnname:
            stolen_refs_by_fnname[fnname].add(argindex)
        else:
            stolen_refs_by_fnname[fnname] = set([argindex])

    gcc.register_attribute('cpychecker_steals_reference_to_arg',
                           1, 1,
                           False, False, False,
                           attribute_callback_for_steals_reference_to_arg)
    gcc.define_macro('WITH_CPYCHECKER_STEALS_REFERENCE_TO_ARG_ATTRIBUTE')

    # Handler for __attribute__((cpychecker_type_object_for_struct(type)))
    # and #ifdef WITH_CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF_ATTRIBUTE
    def attribute_callback_type_object_for_typedef(*args):
        if 0:
            print('attribute_callback_type_object_for_typedef(%r)' % (args, ))
        check_isinstance(args[0], gcc.VarDecl)
        check_isinstance(args[1], gcc.StringCst)
        typedef_name = args[1].constant
        register_type_object(args[0], typedef_name)

    gcc.register_attribute('cpychecker_type_object_for_typedef',
                           1, 1,
                           False, False, False,
                           attribute_callback_type_object_for_typedef)
    gcc.define_macro('WITH_CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF_ATTRIBUTE')

    # Handler for __attribute__((cpychecker_sets_exception))
    # and #ifdef WITH_CPYCHECKER_SETS_EXCEPTION_ATTRIBUTE
    def attribute_callback_for_sets_exception(*args):
        if 0:
            print('attribute_callback_for_sets_exception(%r)' % args)
        check_isinstance(args[0], gcc.FunctionDecl)
        fnname = args[0].name
        fnnames_setting_exception.add(fnname)

    gcc.register_attribute('cpychecker_sets_exception',
                           0, 0,
                           False, False, False,
                           attribute_callback_for_sets_exception)
    gcc.define_macro('WITH_CPYCHECKER_SETS_EXCEPTION_ATTRIBUTE')

    # Handler for __attribute__((cpychecker_negative_result_sets_exception))
    # and #ifdef WITH_CPYCHECKER_NEGATIVE_RESULT_SETS_EXCEPTION_ATTRIBUTE
    def attribute_callback_for_negative_result_sets_exception(*args):
        if 0:
            print('attribute_callback_for_negative_result_sets_exception(%r)' % args)
        check_isinstance(args[0], gcc.FunctionDecl)
        fnname = args[0].name
        fnnames_setting_exception_on_negative_result.add(fnname)

    gcc.register_attribute('cpychecker_negative_result_sets_exception',
                           0, 0,
                           False, False, False,
                           attribute_callback_for_negative_result_sets_exception)
    gcc.define_macro('WITH_CPYCHECKER_NEGATIVE_RESULT_SETS_EXCEPTION_ATTRIBUTE')
