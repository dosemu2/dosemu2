import gdb, gdb.printing, re


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
