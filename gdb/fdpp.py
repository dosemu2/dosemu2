import gdb, gdb.printing, re


def canonical(t):
    try:
        return t.unqualified().strip_typedefs()
    except Exception:
        # older gdb API:
        try:
            return t.unqualified().unaliased()
        except Exception:
            return t


def tag_name(t):
    try:
        return canonical(t).tag
    except Exception:
        return None


def resolve_FarPtr_in_lowmem(fp):
    """Takes a gdb.Value and returns another targetted in lowmem"""

    # infer template param from type string
    name = str(fp.type.strip_typedefs())
    m = re.search(r'^FarPtr.*<\s*([^>\s]+)', name)
    if m:
        tname = m.group(1)
    else:
        raise gdb.GdbError(f"can't determine inner type from '{name}'")

    try:
        vptr = fp['ptr']
        seg = int(vptr['seg'])
        off = int(vptr['off'])
    except:
        raise gdb.GdbError("can't find ptr.{seg,off}") from None

    def lookup(s):
        try:
            return gdb.lookup_type(s)
        except:
            return None

    # try lookup with/without 'struct'
    gdb_t = lookup(tname) or lookup(f"struct {tname}")
    if not gdb_t:
        raise gdb.GdbError(f"type {tname} not found in debug info")

    return gdb.parse_and_eval(f"(({tname} *) &lowmem_base[(({seg:#06x})<<4) + ({off:#06x})])")


############## User defined functions


class FdppPtrFunction(gdb.Function):
    """
    Use as $fdppptr(var) in expressions (extracts struct type from input var)

    For dereferencing FarPtrs e.g.
       (gdb) p *$fdppptr(pdev)

       $24 = {dh_next = {ptr = {off = 2116, seg = 144}}, dh_attr = 32787,
              dh_strategy = {<NearPtr<void, &dosobj_seg>> = {_off = 1257}, <No data fields>},
              dh_interrupt = {<NearPtr<void, &dosobj_seg>> = { _off = 1268}, <No data fields>},
              dh_name = {<MembBase<unsigned char, dhdr,
                  dhdr::(lambda at ../subprojects/libfdpp/hdr/device.h:121:3){},
                  0>> = {static off = 10}, sym = "CON     ", static len = <optimised out>}}

    Essentially the same as:
       (gdb) p *((struct dhdr *) &lowmem_base[(pdev.ptr.seg<<4) + pdev.ptr.off])"""

    def __init__(self):
        super().__init__("fdppptr")

    def invoke(self, ptr):
        return resolve_FarPtr_in_lowmem(ptr)

try:
    _fdppptr = FdppPtrFunction()
except Exception as e:
    print("registration failed:", e)


######################### Pretty Printers below

class FDPPTypePrinter:

    def __init__(self, val):
        self.val = val
        self.classname = type(self).__name__
        self.name = self.classname.replace('Printer', '')

    def children(self):
        return iter(())

    def display_hint(self):
        return "string"



class FarPtrPrinter(FDPPTypePrinter):
    "Printer for FarPtr-like types."

    def to_string(self):
        try:
            #t = canonical(self.val.type)
            #name = tag_name(t) # or str(t)

            # assume self.val.ptr is a struct with fields seg and off
            ptr = self.val['ptr']
            seg = int(ptr['seg'])
            off = int(ptr['off'])
            return f"{self.name} seg={seg:#06x}, off={off:#06x}"  # [{seg:04x}:{off:04x}]"
        except Exception as e:
            return f"{self.classname}: unreadable ptr/seg/off>({e})"


class NearPtrPrinter(FDPPTypePrinter):
    "Printer for NearPtr-like types."

    def to_string(self):
        try:
            #t = canonical(self.val.type)
            #name = tag_name(t) # or str(t)

            off = int(self.val['_off'])
            return f"{self.name} off={off:#06x}"
        except Exception as e:
            return f"{self.classname}: unreadable _off>({e})"


class MembBasePrinter(FDPPTypePrinter):
    "Printer for MembBase-like types."

    def to_string(self):
        try:
            #t = canonical(self.val.type)
            #name = tag_name(t) # or str(t)
            off = int(self.val['off'])
            return f"{self.name} off={off:#06x}"
        except Exception as e:
            return f"{self.classname}: unreadable off>({e})"


class ArMembPrinter(FDPPTypePrinter):
    "Printer for ArMemb-like types."

    def to_string(self):
        try:
            sym = self.val['sym']
            return f"{self.name} sym={sym}"
        except Exception as e:
            return f"{self.classname}: unreadable sym>({e})"


def looks_like_fdpp_type(val):
    name = tag_name(val.type)

    if not name:
        return None

    if "FarPtr" in name:
        return FarPtrPrinter(val)

    if "NearPtr" in name:
        return NearPtrPrinter(val)

    if "MembBase" in name:
        return MembBasePrinter(val)

    if "ArMemb" in name:
        return ArMembPrinter(val)

    return None


def register_printers(objfile=None):
    gdb.printing.register_pretty_printer(objfile, looks_like_fdpp_type, replace=True)


register_printers()
