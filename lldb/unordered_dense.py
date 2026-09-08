"""LLDB data formatters for ankerl::unordered_dense containers.

Adds pretty-printing for `ankerl::unordered_dense::map`, `set`,
`segmented_map`, `segmented_set` (including the `pmr::` variants) and for
`ankerl::unordered_dense::segmented_vector` on its own. One provider covers
every variant, because the public aliases all resolve to
`ankerl::unordered_dense::detail::table<...>`.

The table stores all elements densely in `m_values` -- a `std::vector` for
map/set, a `segmented_vector` for segmented_map/segmented_set -- so the
formatter walks that container directly and never has to decode the robin-hood
bucket array. Elements come out in the container's iteration order (which is
insertion order until something is erased).

    (lldb) frame variable word_count
    (unordered_dense::map<std::string, int>) word_count = size=3 bucket_count=4 {
      ["alpha"] = first="alpha", second=1
      ["beta"] = first="beta", second=2
      ["gamma"] = first="gamma", second=3
    }

Load it in LLDB with

    command script import /path/to/unordered_dense/lldb/unordered_dense.py

or from ~/.lldbinit with the same line. `frame variable -R <var>` still shows
the raw members (m_values, m_buckets, ...) whenever they are needed.

The implementation only reads memory: m_values' begin/end pointers (libstdc++
and libc++ layouts are both known) or segmented_vector's block pointers, then
builds each element with SBValue.CreateValueFromAddress(). It never runs
expressions, so it is safe on core dumps and has no side effects.
"""

import re

try:
    import lldb
except ImportError:
    # Importable without LLDB for tooling (syntax checks, linters).
    lldb = None

# When True (default), map children are named ["key"] instead of [0], [1], ...
# whenever the key renders as a short scalar or string. Toggle with
#   (lldb) script unordered_dense.NAME_CHILDREN_BY_KEY = False
NAME_CHILDREN_BY_KEY = True

_MAX_KEY_NAME_LEN = 36

# segmented_vector's default MaxSegmentSizeBytes, used when the value can't be
# recovered from the type name.
_DEFAULT_SEGMENT_BYTES = 4096

_UINT_FAIL = 0xFFFFFFFFFFFFFFFF

# ankerl::unordered_dense has an inline version namespace (v4_11_0 and
# friends) that shows up in canonical type names; tolerate it. LLDB matches
# type formatters with POSIX extended regexes over the whole type name, so
# patterns must use plain () groups (no (?:...)) and consume every template
# argument.
_NS = r"^ankerl::unordered_dense::(v[0-9]+(_[0-9]+)*::)?"

_TABLE_RES = [
    _NS + r"detail::table<.*>$",
    _NS + r"(pmr::)?(segmented_)?map<.*>$",
    _NS + r"(pmr::)?(segmented_)?set<.*>$",
    # gcc's DWARF names an alias-spelled declaration ("map<...> x;") without
    # template arguments, so the bare names need their own entries.
    _NS + r"(pmr::)?(segmented_)?map$",
    _NS + r"(pmr::)?(segmented_)?set$",
]

_SEGMENTED_VECTOR_RES = [_NS + r"segmented_vector<.*>$"]

_CATEGORY = "unordered-dense"


def _valid(value):
    return value is not None and value.IsValid()


def _member(valobj, *names):
    """Fetch a raw (non-synthetic) member by walking a name path."""
    if not _valid(valobj):
        return None
    current = valobj.GetNonSyntheticValue()
    for name in names:
        if not _valid(current):
            return None
        current = current.GetChildMemberWithName(name)
    return current if _valid(current) else None


def _uint(valobj, fail=None):
    if not _valid(valobj):
        return fail
    value = valobj.GetValueAsUnsigned(_UINT_FAIL)
    return fail if value == _UINT_FAIL else value


def _type_name(valobj):
    if not _valid(valobj):
        return ""
    return valobj.GetTypeName() or valobj.GetDisplayTypeName() or ""


def _vector_begin_end(vec_valobj):
    """Return (begin, end, pointee_type) of a std::vector, or None.

    Knows the libstdc++ (_M_impl._M_start/_M_finish) and libc++
    (__begin_/__end_) layouts, for any element type and allocator -- also the
    pointer-array m_blocks of segmented_vector.
    """
    if not _valid(vec_valobj):
        return None
    raw = vec_valobj.GetNonSyntheticValue()

    begin = raw.GetChildMemberWithName("__begin_")
    end = raw.GetChildMemberWithName("__end_")
    if _valid(begin) and _valid(end) and begin.GetType().IsPointerType() and end.GetType().IsPointerType():
        first = _uint(begin)
        last = _uint(end)
        if first is None or last is None:
            return None
        return first, last, begin.GetType().GetPointeeType()

    impl = raw.GetChildMemberWithName("_M_impl")
    if _valid(impl):
        begin = impl.GetChildMemberWithName("_M_start")
        end = impl.GetChildMemberWithName("_M_finish")
        if _valid(begin) and _valid(end) and begin.GetType().IsPointerType() and end.GetType().IsPointerType():
            first = _uint(begin)
            last = _uint(end)
            if first is None or last is None:
                return None
            return first, last, begin.GetType().GetPointeeType()
    return None


def _sanitized_key_name(key):
    """Turn a rendered key into a child name, or None if it is not simple.

    Accepts scalars (via GetValue) and strings (via a "..." summary), strips
    quoting, drops anything with control characters or brackets that would
    confuse child-name round trips, and caps the length.
    """
    text = None

    summary = key.GetSummary()
    if summary is not None:
        summary = summary.strip()
        if len(summary) >= 2 and summary.startswith('"') and summary.endswith('"'):
            text = re.sub(r"\\(.)", r"\1", summary[1:-1])

    if text is None:
        value = key.GetValue()
        if value is not None and value.strip():
            # Strip integer literal suffixes LLDB appends, e.g. "42ULL" --
            # but never from a hex render, whose trailing f is a digit.
            text = value.strip()
            if not text.lower().startswith("0x"):
                text = re.sub(r"(?:U?LL|L|U|f|F)$", "", text)

    if not text:
        return None

    text = text.replace("]", "").replace("\\", "")
    if not text or any(ord(ch) < 32 or ord(ch) > 126 for ch in text):
        return None
    # [digits] is reserved for index-based children, so a key that renders
    # as digits keeps its index name instead of shadowing (or colliding
    # with) another element's index.
    if text.isdigit():
        return None
    if len(text) > _MAX_KEY_NAME_LEN:
        text = text[:_MAX_KEY_NAME_LEN]
    return "[" + text + "]" if text else None


def _pair_first(pair_valobj):
    """First member of a std::pair in whatever stdlib layout it has."""
    for name in ("first", "__first_"):
        first = pair_valobj.GetNonSyntheticValue().GetChildMemberWithName(name)
        if _valid(first):
            return first
    return None


class _Storage(object):
    """Parsed view of a values container (std::vector or segmented_vector).

    Produces the element count, element type, element size, and the address of
    any element index, reading only memory the container itself points to.

    Dispatch is structural (which members exist), not by type name: LLDB
    mis-reports the type of m_values/m_buckets -- fields whose types come
    through conditional_t/decltype chains resolve to the enclosing table --
    so type names and template arguments of these values cannot be trusted.
    """

    def __init__(self, valobj, values):
        self.valobj = valobj
        self.kind = "unknown"
        self.count = 0
        self.elem_type = None
        self.elem_size = 0
        self._vector_begin = 0
        self._blocks_begin = 0
        self._pointer_size = 0
        self._block_cache = {}
        self._elems_per_block = 0

        if _valid(_member(values, "m_size")) and _valid(_member(values, "m_blocks")):
            self._parse_segmented(values)
        else:
            self._parse_vector(values)

    def _parse_vector(self, values):
        layout = _vector_begin_end(values)
        if layout is None:
            return
        begin, end, pointee = layout
        if not _valid(pointee):
            return
        elem_size = pointee.GetByteSize()
        if not elem_size:
            return
        span = end - begin
        if span < 0 or span % elem_size != 0 or span > (1 << 44):
            # Refuse impossible layouts rather than show wrong data.
            return
        self.kind = "vector"
        self._vector_begin = begin
        self.elem_type = pointee
        self.elem_size = elem_size
        self.count = span // elem_size

    def _parse_segmented(self, values):
        size = _uint(_member(values, "m_size"))
        if size is None:
            return
        blocks = _member(values, "m_blocks")
        layout = _vector_begin_end(blocks) if _valid(blocks) else None
        if layout is None:
            return
        # Element type from m_blocks' data pointer: T** -> T. The type name
        # of m_values itself cannot be trusted (see class docstring).
        elem_type = layout[2].GetPointeeType()
        if not _valid(elem_type):
            return
        elem_size = elem_type.GetByteSize()
        if not elem_size:
            return

        segment_bytes = _Storage._segment_bytes(_type_name(values))
        self._elems_per_block = _Storage._elements_per_block(elem_size, segment_bytes)

        # Block pointers are read lazily in address_of(): LLDB only fetches
        # the children it displays, and a huge map owns thousands of blocks.
        blocks_begin, blocks_end, _ = layout
        pointer_size = self.valobj.GetTarget().GetAddressByteSize()
        span = blocks_end - blocks_begin
        if span < 0 or span % pointer_size != 0 or span > (1 << 34):
            return

        self.kind = "segmented"
        self.count = size
        self.elem_type = elem_type
        self.elem_size = elem_size
        self._blocks_begin = blocks_begin
        self._pointer_size = pointer_size

    def _block_address(self, block_index):
        if block_index not in self._block_cache:
            process = self.valobj.GetProcess()
            error = lldb.SBError()
            addr = process.ReadPointerFromMemory(self._blocks_begin + block_index * self._pointer_size, error)
            self._block_cache[block_index] = addr if error.Success() else 0
        return self._block_cache[block_index]

    @staticmethod
    def _elements_per_block(elem_size, segment_bytes):
        # Mirrors segmented_vector::num_bits_closest(), which counts how far
        # sizeof(T) can be shifted left while staying inside the segment.
        bits = 0
        while (elem_size << (bits + 1)) <= segment_bytes:
            bits += 1
        return 1 << bits

    @staticmethod
    def _segment_bytes(type_name):
        """MaxSegmentSizeBytes from the type name ('..., 4096UL>'), else 4096.

        A value below sizeof(T) is accepted: it is a legal, if degenerate,
        spelling the header answers with one element per block."""
        match = re.search(r",\s*(\d+)\s*[uUlL]*\s*>\s*$", type_name)
        if match:
            value = int(match.group(1))
            if value > 0:
                return value
        return _DEFAULT_SEGMENT_BYTES

    def address_of(self, index):
        if index < 0 or index >= self.count:
            return None
        if self.kind == "vector":
            return self._vector_begin + index * self.elem_size
        if self.kind == "segmented":
            block = self._block_address(index >> self._log2(self._elems_per_block))
            if not block:
                return None
            return block + (index & (self._elems_per_block - 1)) * self.elem_size
        return None

    @staticmethod
    def _log2(power_of_two):
        return power_of_two.bit_length() - 1


class DenseTableProvider(object):
    """Synthetic children for ankerl::unordered_dense tables.

    Children are the densely stored elements of m_values: one std::pair per
    map entry, one key per set entry, in insertion order.
    """

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.storage = None
        self.is_map = False
        self._names = {}
        self._fallback = None

    def update(self):
        self.storage = None
        self._names = {}
        self._fallback = None
        self.is_map = False

        values = _member(self.valobj, "m_values")
        if not _valid(values):
            return False

        storage = _Storage(self.valobj, values)
        if storage.kind == "unknown":
            # Custom value container (deque, interprocess vector, ...): hand
            # out whatever children LLDB itself can produce for it.
            self._fallback = self.valobj.GetChildMemberWithName("m_values")
        else:
            self.storage = storage
        self.is_map = self._detect_map(values)
        return True

    def _detect_map(self, values):
        # detail::table's second template argument is void exactly for sets;
        # aliases (map/set/segmented_*/pmr::*) keep the same argument order.
        try:
            table_arg_t = self.valobj.GetType().GetTemplateArgumentType(1)
            if _valid(table_arg_t):
                name = (table_arg_t.GetName() or "").strip()
                if name:
                    return name != "void"
        except Exception:
            pass
        elem_type = self.storage.elem_type if self.storage is not None else None
        if not _valid(elem_type):
            elem_type = values.GetType().GetTemplateArgumentType(0)
        if not _valid(elem_type):
            return False
        # Ambiguous on purpose: without template arguments (gcc's bare-alias
        # DWARF) a set<std::pair<...>> is indistinguishable from a map -- its
        # elements are pairs too -- and gets pair-first child naming. Values
        # stay correct; only the names differ from a set's own-element naming.
        return "pair<" in (elem_type.GetName() or "")

    def num_children(self):
        if self.storage is not None:
            return self.storage.count
        if _valid(self._fallback):
            return self._fallback.GetNumChildren()
        return 0

    def might_have_children(self):
        return self.num_children() > 0

    def get_child_index(self, name):
        if name in self._names:
            return self._names[name]
        match = re.match(r"^\[(\d+)\]$", name)
        return int(match.group(1)) if match else None

    def get_child_at_index(self, index):
        if self.storage is None:
            if _valid(self._fallback):
                return self._fallback.GetChildAtIndex(index)
            return None

        address = self.storage.address_of(index)
        if address is None:
            return None

        element = self.valobj.CreateValueFromAddress("[{}]".format(index), address, self.storage.elem_type)
        if not _valid(element):
            return None

        if NAME_CHILDREN_BY_KEY:
            name = self._key_name(element)
            # First key to claim a name keeps it; a later key that sanitizes
            # to the same name falls back to its index, so child names stay
            # unique and ["key"] lookup stays unambiguous.
            if name is not None and name not in self._names:
                self._names[name] = index
                element = self.valobj.CreateValueFromAddress(name, address, self.storage.elem_type)
        return element

    def _key_name(self, element):
        if self.is_map:
            first = _pair_first(element)
            return _sanitized_key_name(first) if first is not None else None
        return _sanitized_key_name(element.GetNonSyntheticValue())


class SegmentedVectorProvider(object):
    """Synthetic children for ankerl::unordered_dense::segmented_vector."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.storage = None

    def update(self):
        self.storage = _Storage(self.valobj, self.valobj)
        return True

    def num_children(self):
        return self.storage.count if self.storage is not None else 0

    def might_have_children(self):
        return self.num_children() > 0

    def get_child_index(self, name):
        match = re.match(r"^\[(\d+)\]$", name)
        return int(match.group(1)) if match else None

    def get_child_at_index(self, index):
        if self.storage is None:
            return None
        address = self.storage.address_of(index)
        if address is None:
            return None
        return self.valobj.CreateValueFromAddress("[{}]".format(index), address, self.storage.elem_type)


def _bucket_count(valobj, size):
    """0 when no buckets are allocated, else m_bucket_mask + 1 -- what
    table::bucket_count() reports. An unparseable bucket container (custom,
    or fancy-pointer) is answered from the mask instead of assumed empty,
    so None means even the mask is unreadable. With the member itself
    unreadable and no values, 0 -- the never-reserve()d default."""
    buckets = _member(valobj, "m_buckets")
    allocated = None
    if _valid(buckets):
        if _valid(_member(buckets, "m_blocks")):
            seg_size = _uint(_member(buckets, "m_size"))
            if seg_size is not None:
                allocated = seg_size > 0
        else:
            layout = _vector_begin_end(buckets)
            if layout is not None:
                allocated = layout[1] != layout[0]
        if allocated is False:
            return 0
    elif not size:
        return 0
    mask = _uint(_member(valobj, "m_bucket_mask"))
    return None if mask is None else mask + 1


def table_summary(valobj, internal_dict):
    values = _member(valobj, "m_values")
    if not _valid(values):
        return "<unordered_dense table>"
    storage = _Storage(valobj, values)
    size = storage.count if storage.kind != "unknown" else None
    buckets = _bucket_count(valobj, size)

    text = "size={}".format(size if size is not None else "?")
    if buckets is not None:
        text += " bucket_count={}".format(buckets)
    return text


def segmented_vector_summary(valobj, internal_dict):
    size = _uint(_member(valobj, "m_size"))
    return "size={}".format(size) if size is not None else "<segmented_vector>"


def __lldb_init_module(debugger, internal_dict):
    category = debugger.CreateCategory(_CATEGORY)
    for pattern in _TABLE_RES:
        category.AddTypeSynthetic(
            lldb.SBTypeNameSpecifier(pattern, True),
            lldb.SBTypeSynthetic.CreateWithClassName("{}.DenseTableProvider".format(__name__)),
        )
        category.AddTypeSummary(
            lldb.SBTypeNameSpecifier(pattern, True),
            lldb.SBTypeSummary.CreateWithFunctionName("{}.table_summary".format(__name__)),
        )
    for pattern in _SEGMENTED_VECTOR_RES:
        category.AddTypeSynthetic(
            lldb.SBTypeNameSpecifier(pattern, True),
            lldb.SBTypeSynthetic.CreateWithClassName("{}.SegmentedVectorProvider".format(__name__)),
        )
        category.AddTypeSummary(
            lldb.SBTypeNameSpecifier(pattern, True),
            lldb.SBTypeSummary.CreateWithFunctionName("{}.segmented_vector_summary".format(__name__)),
        )
    category.SetEnabled(True)
