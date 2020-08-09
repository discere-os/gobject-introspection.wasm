# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2008  Johan Dahlin
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

import os
import tempfile
import io
import codecs

from collections import OrderedDict

import pycparser.c_generator

import pcpp
import pycparser

from .libtoolimporter import LibtoolImporter
from .message import Position
from .ccompiler import CCompiler
from .utils import have_debug_flag, dll_dirs

with LibtoolImporter(None, None):
    dlldirs = dll_dirs()
    dlldirs.add_dll_dirs(['gio-2.0'])
    if 'UNINSTALLED_INTROSPECTION_SRCDIR' in os.environ:
        from _giscanner import SourceScanner as CSourceScanner
    else:
        from giscanner._giscanner import SourceScanner as CSourceScanner
    dlldirs.cleanup_dll_dirs()


class EvalException(Exception):
    pass


class LenientPreprocessor(pcpp.Preprocessor):
    def on_include_not_found(self, is_system_include, curdir, includepath):
        raise pcpp.OutputDirective(pcpp.Action.IgnoreAndRemove)


def _evaluate_rank(value):
    if isinstance(value, float):
        return 3
    elif isinstance(value, int):
        return 2
    elif isinstance(value, bool):
        return 1
    return 0


def _implicit_type(value):
    if isinstance(value, float):
        return 'gdouble'
    elif isinstance(value, int):
        return 'gint'
    elif isinstance(value, bool):
        return 'gboolean'
    elif isinstance(value, str):
        return 'gchar*'
    raise EvalException


def _strip_qualifiers(expr):
    if hasattr(expr, 'quals'):
        expr.quals = []
    for c in expr:
        _strip_qualifiers(c)


def _truncate(val, typename):
    if typename in ('guint8', 'guchar', 'unsigned char'):
        res = val % 2 ** 8
    elif typename in ('guint16', 'gushort', 'unsigned short'):
        res = val % 2 ** 16
    elif typename in ('guint32', 'guint', 'unsigned int'):
        res = val % 2 ** 32
    elif typename in ('guint64', 'unsigned long long'):
        res = val % 2 ** 64
    elif typename in ('gdouble', 'double', 'gfloat', 'float'):
        res = float(val)
    elif typename == 'gboolean':
        res = bool(val)
    else:
        res = val

    return res


def _evaluate(expr, typedefs):
    if isinstance(expr, pycparser.c_ast.Constant) and expr.type.split(' ')[-1] in 'int':
        typename = None
        val = expr.value

        if val.endswith(('ull', 'ULL', 'ul', 'UL')):
            typename = 'guint64'
        elif val.endswith(('l', 'L', 'll', 'LL')):
            typename = 'gint64'
        elif val.endswith(('u', 'U')):
            typename = 'guint32'

        val = val.rstrip('uUlL')

        if val.startswith('0x'):
            base = 16
        elif val.startswith('0b'):
            base = 2
        elif val.startswith('0'):
            base = 8
        else:
            base = 10

        return (int(val, base=base), typename)
    elif isinstance(expr, pycparser.c_ast.Constant) and expr.type == 'double':
        return (float(expr.value), None)
    elif isinstance(expr, pycparser.c_ast.Constant) and expr.type == 'string':
        ret = expr.value.strip('"')
        try:
            s, l = codecs.escape_decode(ret)
            ret = s.decode('utf-8')
        except UnicodeDecodeError:
            pass
        return (ret, None)
    elif isinstance(expr, pycparser.c_ast.Constant) and expr.type == 'char':
        return (ord(expr.value.strip("'")), None)
    elif isinstance(expr, pycparser.c_ast.Cast):
        res, _ = _evaluate(expr.expr, typedefs)

        generator = pycparser.c_generator.CGenerator()
        # Don't carry over qualifiers
        _strip_qualifiers(expr.to_type)
        typename = generator._generate_type(expr.to_type)

        unaliased = typedefs.get(typename, typename)

        res = _truncate(res, unaliased)

        return (res, typename)
    elif isinstance(expr, pycparser.c_ast.BinaryOp):
        left, left_type = _evaluate(expr.left, typedefs)
        right, right_type = _evaluate(expr.right, typedefs)

        left_rank = _evaluate_rank(left)
        right_rank = _evaluate_rank(right)

        # eg. (gfloat) 42 + (guint8) 43
        if left_rank > right_rank:
            typename = left_type or _implicit_type(left)
        # eg. (gboolean) TRUE + (guint8) 43
        elif left_rank < right_rank:
            typename = right_type or _implicit_type(right)
        # eg. 42 + 43, but also (guint8) 42 + (guint64) 3850
        else:
            # We could go even further down the ranking rabbit hole
            # and start looking at the sizeof(typename), but at this
            # point we stop pretending to be a compiler
            typename = None

        if expr.op == '+':
            res = left + right
        elif expr.op == '-':
            res = left - right
        elif expr.op == '*':
            res = left * right
        elif expr.op == '/':
            res = left / right
            if isinstance(left, int) and isinstance(right, int):
                res = int(res)
        elif expr.op == '<<':
            res = left << right
        elif expr.op == '>>':
            res = left >> right
        elif expr.op == '&':
            res = left & right
        elif expr.op == '|':
            res = left | right
        elif expr.op == '^':
            res = left ^ right
        else:
            raise EvalException

        return (res, typename)
    elif isinstance(expr, pycparser.c_ast.UnaryOp):
        res, typename = _evaluate(expr.expr, typedefs)
        if expr.op == '-':
            return (-1 * res, typename)
        elif expr.op == '~':
            res = _truncate(~res, typename)
            return (res, typename)
        elif expr.op == '!':
            type_ = type(res)
            return (type_(not res), typename)
    else:
        raise EvalException


def bool_cast():
    preproc = pcpp.Preprocessor()
    tokens = []
    preproc.parse('(gboolean)')
    while True:
        tok = preproc.token()
        if tok is None:
            break
        tokens.append(tok)
    return tokens[:3]


# Temporary while we're still using our home-grown C parser,
# once we switch to pycparser for non-macro symbols we can
# decide on a more elegant solution
#
# This is to make cases such as REGRESS_GUINT64_CONSTANTA and
# REGRESS_FOO_FLAGS_SECOND_AND_THIRD pass (grep in the test suite)
def visit_typedefs(symbols):
    from giscanner.gi_ast import INTEGER_TYPES, FLOATING_TYPES
    extra_typedefs = OrderedDict()
    enum_member_values = {}
    basic_ctypes = [t.ctype for t in INTEGER_TYPES + FLOATING_TYPES]
    for s in symbols:
        if s.type == CSYMBOL_TYPE_TYPEDEF:
            if s.base_type.type == CTYPE_TYPEDEF and s.base_type.name in basic_ctypes:
                extra_typedefs[s.ident] = s.base_type.name
            elif s.base_type.type == CTYPE_ENUM:
                for child in s.base_type.child_list:
                    enum_member_values[child.ident] = child.const_int
        elif s.type == CSYMBOL_TYPE_ENUM:
            for child in s.base_type.child_list:
                enum_member_values[child.ident] = child.const_int

    return (extra_typedefs, enum_member_values)


HEADER_EXTS = ['.h', '.hpp', '.hxx']
SOURCE_EXTS = ['.c', '.cpp', '.cc', '.cxx']
ALL_EXTS = SOURCE_EXTS + HEADER_EXTS

(CSYMBOL_TYPE_INVALID,
 CSYMBOL_TYPE_ELLIPSIS,
 CSYMBOL_TYPE_CONST,
 CSYMBOL_TYPE_OBJECT,
 CSYMBOL_TYPE_FUNCTION,
 CSYMBOL_TYPE_FUNCTION_MACRO,
 CSYMBOL_TYPE_STRUCT,
 CSYMBOL_TYPE_UNION,
 CSYMBOL_TYPE_ENUM,
 CSYMBOL_TYPE_TYPEDEF,
 CSYMBOL_TYPE_MEMBER) = range(11)

(CTYPE_INVALID,
 CTYPE_VOID,
 CTYPE_BASIC_TYPE,
 CTYPE_TYPEDEF,
 CTYPE_STRUCT,
 CTYPE_UNION,
 CTYPE_ENUM,
 CTYPE_POINTER,
 CTYPE_ARRAY,
 CTYPE_FUNCTION) = range(10)

STORAGE_CLASS_NONE = 0
STORAGE_CLASS_TYPEDEF = 1 << 1
STORAGE_CLASS_EXTERN = 1 << 2
STORAGE_CLASS_STATIC = 1 << 3
STORAGE_CLASS_AUTO = 1 << 4
STORAGE_CLASS_REGISTER = 1 << 5
STORAGE_CLASS_THREAD_LOCAL = 1 << 6

TYPE_QUALIFIER_NONE = 0
TYPE_QUALIFIER_CONST = 1 << 1
TYPE_QUALIFIER_RESTRICT = 1 << 2
TYPE_QUALIFIER_VOLATILE = 1 << 3
TYPE_QUALIFIER_EXTENSION = 1 << 4

FUNCTION_NONE = 0
FUNCTION_INLINE = 1 << 1

(UNARY_ADDRESS_OF,
 UNARY_POINTER_INDIRECTION,
 UNARY_PLUS,
 UNARY_MINUS,
 UNARY_BITWISE_COMPLEMENT,
 UNARY_LOGICAL_NEGATION) = range(6)


def symbol_type_name(symbol_type):
    return {
        CSYMBOL_TYPE_INVALID: 'invalid',
        CSYMBOL_TYPE_ELLIPSIS: 'ellipsis',
        CSYMBOL_TYPE_CONST: 'const',
        CSYMBOL_TYPE_OBJECT: 'object',
        CSYMBOL_TYPE_FUNCTION: 'function',
        CSYMBOL_TYPE_FUNCTION_MACRO: 'function_macro',
        CSYMBOL_TYPE_STRUCT: 'struct',
        CSYMBOL_TYPE_UNION: 'union',
        CSYMBOL_TYPE_ENUM: 'enum',
        CSYMBOL_TYPE_TYPEDEF: 'typedef',
        CSYMBOL_TYPE_MEMBER: 'member'}.get(symbol_type)


def ctype_name(ctype):
    return {
        CTYPE_INVALID: 'invalid',
        CTYPE_VOID: 'void',
        CTYPE_BASIC_TYPE: 'basic',
        CTYPE_TYPEDEF: 'typedef',
        CTYPE_STRUCT: 'struct',
        CTYPE_UNION: 'union',
        CTYPE_ENUM: 'enum',
        CTYPE_POINTER: 'pointer',
        CTYPE_ARRAY: 'array',
        CTYPE_FUNCTION: 'function'}.get(ctype)


class SourceType(object):
    __members__ = ['type', 'base_type', 'name', 'type_qualifier',
                   'child_list', 'is_bitfield']

    def __init__(self, scanner, stype):
        self._scanner = scanner
        self._stype = stype

    def __repr__(self):
        return "<%s type='%s' name='%s'>" % (
            self.__class__.__name__,
            ctype_name(self.type),
            self.name)

    @property
    def type(self):
        return self._stype.type

    @property
    def base_type(self):
        if self._stype.base_type is not None:
            return SourceType(self._scanner, self._stype.base_type)

    @property
    def name(self):
        return self._stype.name

    @property
    def type_qualifier(self):
        return self._stype.type_qualifier

    @property
    def child_list(self):
        for symbol in self._stype.child_list:
            if symbol is None:
                continue
            yield SourceSymbol(self._scanner, symbol)

    @property
    def is_bitfield(self):
        return self._stype.is_bitfield


class SourceSymbol(object):
    __members__ = ['const_int', 'const_double', 'const_string', 'const_boolean',
                   'ident', 'type', 'base_type']

    def __init__(self, scanner, symbol):
        self._scanner = scanner
        self._symbol = symbol

    def __repr__(self):
        src = self.source_filename
        if src:
            line = self.line
            if line:
                src += ":'%s'" % (line, )
        return "<%s type='%s' ident='%s' src='%s'>" % (
            self.__class__.__name__,
            symbol_type_name(self.type),
            self.ident,
            src)

    @property
    def const_int(self):
        return self._symbol.const_int

    @property
    def const_double(self):
        return self._symbol.const_double

    @property
    def const_string(self):
        return self._symbol.const_string

    @property
    def const_boolean(self):
        return self._symbol.const_boolean

    @property
    def ident(self):
        return self._symbol.ident

    @property
    def type(self):
        return self._symbol.type

    @property
    def base_type(self):
        if self._symbol.base_type is not None:
            return SourceType(self._scanner, self._symbol.base_type)

    @property
    def source_filename(self):
        return self._symbol.source_filename

    @property
    def line(self):
        return self._symbol.line

    @property
    def private(self):
        return self._symbol.private

    @property
    def position(self):
        return Position(self._symbol.source_filename,
                        self._symbol.line)


class SourceScanner(object):

    def __init__(self):
        self._scanner = CSourceScanner()
        self._filenames = []
        self._cpp_options = []
        self._macros = []

    # Public API

    def set_cpp_options(self, includes, defines, undefines, cflags=[]):
        self._cpp_options.extend(cflags)
        for prefix, args in [('-I', [os.path.realpath(f) for f in includes]),
                             ('-D', defines),
                             ('-U', undefines)]:
            for arg in (args or []):
                opt = prefix + arg
                if opt not in self._cpp_options:
                    self._cpp_options.append(opt)

    def parse_files(self, filenames):
        for filename in filenames:
            # self._scanner expects file names to be canonicalized and symlinks to be resolved
            filename = os.path.realpath(filename)
            self._scanner.append_filename(filename)
            self._filenames.append(filename)

        headers = []
        for filename in self._filenames:
            if os.path.splitext(filename)[1] in SOURCE_EXTS:
                self._scanner.lex_filename(filename)
            else:
                headers.append(filename)

        self._parse(headers)

    def parse_macros(self, filenames):
        from giscanner.gi_ast import INTEGER_TYPES, FLOATING_TYPES

        eval_typedefs = '\n'.join([
            'typedef int {};'.format(t.ctype) for t in INTEGER_TYPES
        ])

        eval_typedefs += '\n'.join([''] + [
            'typedef float {};'.format(t.ctype) for t in FLOATING_TYPES
        ])

        extra_typedefs, enum_member_values = visit_typedefs(self.get_symbols())
        bool_cast_tokens = bool_cast()
        preproc = LenientPreprocessor()
        preproc.define('__GI_SCANNER__ 1')
        # Don't let gmacros.h redefine inline
        preproc.define('__STDC_VERSION__ 199900')
        # This is only defined if __GI_SCANNER__ isn't, don't let this throw
        # pycparser off
        preproc.define('G_STATIC_ASSERT(expr)')

        preproc.define('G_HAS_CONSTRUCTORS 1')

        srcdir = os.path.dirname(__file__)

        preproc.add_path(os.path.join(srcdir, 'fake_libc_include'))

        # FIXME: ffi.h is in /usr/include but the gir-girepository target should
        # explicitly state it depends on it as we don't pick up system includes
        # The architecture define is needed to have ffi.h proceed with further
        # inclusions.
        preproc.define('__x86_64__ 1')

        for cflag in self._cpp_options:
            if cflag.startswith('-D'):
                stridx = cflag.find('=')
                if stridx > -1:
                    define = '{} {}'.format(cflag[2:stridx], cflag[stridx + 1:])
                else:
                    define = '{} 1'.format(cflag[2:])
                preproc.define(define)
            elif cflag.startswith('-U'):
                preproc.undef(cflag[2:])
            elif cflag.startswith('-I'):
                preproc.add_path(cflag[2:])

        oh = io.StringIO()

        for filename in filenames:
            with open(filename, 'r', encoding='utf8') as _:
                preproc.parse(_, filename)

            preproc.write(oh)

        parser = pycparser.c_parser.CParser()
        try:
            ast = parser.parse(oh.getvalue(), filename='<source>')
        except pycparser.plyparser.ParseError:
            # TODO: surface an error
            pass
        # TODO: the SourceScanner.parse_macros() code path is entirely
        # replaced by the code below, use the ast parsed above to replace
        # the SourceScanner.parse_symbols() code path and get entirely
        # rid of SourceScanner and its dependencies (flex, bison, the glib
        # circular dependency)

        typedefs = '\n'.join((
            eval_typedefs,
            '\n'.join(['typedef {} {};'.format(v, k) for k, v in extra_typedefs.items()])))

        parser = pycparser.c_parser.CParser()
        for macro in preproc.macros.values():
            macro.source = os.path.abspath(macro.source)
            if macro.source in filenames:
                if macro.arglist is None:
                    macro.evaluated = None
                    # Insert explicit casts for TRUE and FALSE, these will carry over
                    # through macro expansion to expression evaluation, where we can
                    # pick up on the typing hint
                    # eg. #define SOME_VALUE TRUE
                    tokens = []
                    for tok in macro.value:
                        if tok.type == 'CPP_ID':
                            if tok.value in ('TRUE', 'FALSE'):
                                tokens += bool_cast_tokens
                            elif tok.value in enum_member_values:
                                tok.type = 'CPP_INTEGER'
                                tok.value = str(enum_member_values[tok.value])
                        if tok.type != 'CPP_LINECONT':
                            tokens.append(tok)
                    tokens = preproc.expand_macros(tokens)
                    # The type of the left hand side of the assignment (void) is absolutely
                    # irrelevant
                    value = 'void foo={};'.format(''.join([v.value for v in tokens]))
                    try:
                        ast = parser.parse('\n'.join((typedefs, value)), filename='<source>')
                        try:
                            value, typename = _evaluate(ast.ext[-1].init, extra_typedefs)
                            typename = typename or _implicit_type(value)
                            macro.evaluated = (value, typename)
                        except EvalException:
                            pass
                    except pycparser.plyparser.ParseError:
                        pass
                self._macros.append(macro)

    def get_symbols(self):
        for symbol in self._scanner.get_symbols():
            yield SourceSymbol(self._scanner, symbol)

    def get_macros(self):
        return self._macros

    def get_comments(self):
        return self._scanner.get_comments()

    def get_errors(self):
        return self._scanner.get_errors()

    def dump(self):
        print('-' * 30)
        for symbol in self._scanner.get_symbols():
            print(symbol.ident, symbol.base_type.name, symbol.type)

    # Private

    def _parse(self, filenames):
        if not filenames:
            return

        defines = ['__GI_SCANNER__']
        undefs = []

        cc = CCompiler()

        tmp_fd_cpp, tmp_name_cpp = tempfile.mkstemp(prefix='g-ir-cpp-',
                                                    suffix='.c',
                                                    dir=os.getcwd())
        with os.fdopen(tmp_fd_cpp, 'wb') as fp_cpp:
            self._write_preprocess_src(fp_cpp, defines, undefs, filenames)

        tmpfile_basename = os.path.basename(os.path.splitext(tmp_name_cpp)[0])

        # Output file name of the preprocessor, only really used on non-MSVC,
        # so we want the name to match the output file name of the MSVC preprocessor
        tmpfile_output = tmpfile_basename + '.i'

        cc.preprocess(tmp_name_cpp,
                      tmpfile_output,
                      self._cpp_options)

        if not have_debug_flag('save-temps'):
            os.unlink(tmp_name_cpp)
        self._scanner.parse_file(tmpfile_output)
        if not have_debug_flag('save-temps'):
            os.unlink(tmpfile_output)

    def _write_preprocess_src(self, fp, defines, undefs, filenames):
        # Write to the temp file for feeding into the preprocessor
        for define in defines:
            fp.write(('#ifndef %s\n' % (define, )).encode())
            fp.write(('# define %s\n' % (define, )).encode())
            fp.write('#endif\n'.encode())
        for undef in undefs:
            fp.write(('#undef %s\n' % (undef, )).encode())
        for filename in filenames:
            fp.write(('#include <%s>\n' % (filename, )).encode())
