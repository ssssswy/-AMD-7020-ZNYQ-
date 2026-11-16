"""v3.5 read-only ELF32/ARM + map + maintained-source memory checker.

No pyelftools, compiler, board connection, or generated BSP mutation is used.
All intervals in the report are half-open. This checks link-time reservation,
not runtime stack high-water marks, DMA coherency, or physical DDR health.
"""
from __future__ import annotations

import argparse
import ast
import hashlib
import json
import re
import struct
from pathlib import Path


class MemoryContractError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise MemoryContractError(message)


def interval(base, size, label):
    require(0 <= base < 2**32 and 0 <= size <= 2**32 and base + size <= 2**32,
            f"32-bit interval overflow: {label}")
    return base, base + size


def nonoverlap(ranges):
    ordered = sorted((a, b, name) for name, a, b in ranges if b > a)
    for left, right in zip(ordered, ordered[1:]):
        require(left[1] <= right[0], f"Overlap: {left[2]} and {right[2]}")


def parse_elf(data):
    """Parse bounded little-endian ELF32, retaining LOADs/sections/symbols."""
    require(len(data) >= 52 and data[:7] == b"\x7fELF\x01\x01\x01", "Not ELF32 little-endian v1")
    h = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    typ, machine, version, entry, phoff, shoff, flags, ehsize, phsize, phnum, shsize, shnum, shstr = h
    require(typ == 2 and machine == 40 and version == 1 and ehsize == 52, "Not ARM EXEC ELF32")
    require(phsize == 32 and shsize == 40 and 0 < shnum < 65535 and shstr < shnum,
            "Unsupported/invalid ELF table sizes or extended indexes")

    def bounded(offset, size, name):
        require(0 <= offset <= len(data) and 0 <= size <= len(data) - offset, f"Truncated {name}")
        return data[offset:offset + size]

    bounded(phoff, phnum * phsize, "program headers")
    bounded(shoff, shnum * shsize, "section headers")
    raw = [struct.unpack_from("<10I", data, shoff + n * 40) for n in range(shnum)]
    names = bounded(raw[shstr][4], raw[shstr][5], "section names")

    def string(table, offset):
        require(offset < len(table), "String-table index out of range")
        end = table.find(b"\0", offset)
        require(end >= 0, "Unterminated ELF string")
        return table[offset:end].decode("utf-8", errors="strict")

    sections = []
    for n, row in enumerate(raw):
        name, stype, sf, address, offset, size, link, info, align, entsize = row
        interval(address, size, f"section {n}")
        require(align == 0 or align & (align - 1) == 0, "Non-power-of-two section alignment")
        require(not (sf & 2) or not align or address % align == 0, "Misaligned ALLOC section")
        if stype != 8:
            bounded(offset, size, f"section {n}")
        sections.append(dict(index=n, name=string(names, name), type=stype, flags=sf,
                             base=address, size=size, offset=offset, align=align,
                             link=link, entsize=entsize))
    loads = []
    for n in range(phnum):
        ptype, offset, virtual, physical, files, memory, pf, align = struct.unpack_from("<8I", data, phoff + n * 32)
        if ptype != 1:
            continue
        interval(virtual, memory, f"LOAD {n}")
        bounded(offset, files, f"LOAD {n}")
        require(virtual == physical and 0 <= files <= memory and memory > 0, "Invalid LOAD physical/file/memory extent")
        require(align > 0 and align & (align - 1) == 0 and offset % align == virtual % align,
                "Invalid LOAD alignment")
        loads.append(dict(base=virtual, size=memory, file_bytes=files, offset=offset, flags=pf, align=align))
    symbols = {}
    for sec in sections:
        if sec["type"] != 2:
            continue
        require(sec["entsize"] == 16 and sec["size"] % 16 == 0 and sec["link"] < len(sections), "Invalid symbol table")
        linked = sections[sec["link"]]
        require(linked["type"] == 3, "Symbol names are not STRTAB")
        strings = bounded(linked["offset"], linked["size"], "symbol names")
        for off in range(sec["offset"], sec["offset"] + sec["size"], 16):
            name, value, size, info, other, shndx = struct.unpack_from("<IIIBBH", data, off)
            name = string(strings, name)
            if name and shndx:
                symbols.setdefault(name, []).append(dict(base=value, size=size, section=shndx, info=info))
    return dict(entry=entry, flags=flags, loads=loads, sections=sections, symbols=symbols)


def validate_elf_layout(elf, map_text, require_previous_plan=True):
    require(elf["entry"] == 0x100000, "Unexpected ELF entry")
    loads = sorted(elf["loads"], key=lambda x: x["base"])
    nonoverlap([(f"LOAD{n}", *interval(x["base"], x["size"], "LOAD")) for n, x in enumerate(loads)])
    history = [(0x06000000, 0x00800000), (0x06800000, 0x00800000)]
    program = []
    for load in loads:
        if (load["base"], load["size"]) in history:
            require(load["file_bytes"] == 0, "History LOAD contains file data")
        else:
            require(0x100000 <= load["base"] < load["base"] + load["size"] <= 0x800000,
                    "Unknown LOAD or ELF overlaps reserved DDR")
            program.append(load)
    require(program and program[0]["base"] == 0x100000, "Missing program LOAD at 1MiB")
    require(sum((x["base"], x["size"]) in history for x in loads) == 2, "Missing exact history LOADs")
    alloc = [s for s in elf["sections"] if s["flags"] & 2 and s["size"]]
    nonoverlap([(s["name"], *interval(s["base"], s["size"], s["name"])) for s in alloc])
    for s in alloc:
        owners = [p for p in loads if p["base"] <= s["base"] and s["base"] + s["size"] <= p["base"] + p["size"]]
        require(len(owners) == 1, f"Unowned/multiply owned ALLOC section: {s['name']}")
        if s["type"] != 8:
            p = owners[0]
            require(p["offset"] <= s["offset"] and s["offset"] + s["size"] <= p["offset"] + p["file_bytes"] and
                    s["base"] - p["base"] == s["offset"] - p["offset"], f"ALLOC file mapping mismatch: {s['name']}")

    def section(name):
        found = [s for s in alloc if s["name"] == name]
        require(len(found) == 1, f"Missing/duplicate section {name}")
        return found[0]

    for name, (base, size) in zip((".capture_history", ".trigger_history"), history):
        s = section(name)
        require((s["base"], s["size"], s["type"]) == (base, size, 8) and s["align"] >= 64,
                f"History NOBITS/alignment contract changed: {name}")
    for name in (".bss", ".heap", ".stack"):
        require(section(name)["type"] == 8, f"{name} is not NOBITS")

    def sym(name, mapped=False):
        values = elf["symbols"].get(name, [])
        require(len(values) == 1, f"Missing/ambiguous symbol {name}")
        result = values[0]
        if mapped:
            matches = re.findall(r"(?m)^\s*(0x[0-9a-fA-F]+)\s+" + re.escape(name) + r"\s*=", map_text)
            require(matches and all(int(x, 16) == result["base"] for x in matches), f"MAP symbol mismatch: {name}")
        return result

    sizes = {"_HEAP_SIZE":8192, "_STACK_SIZE":8192, "_IRQ_STACK_SIZE":1024,
             "_SUPERVISOR_STACK_SIZE":2048, "_ABORT_STACK_SIZE":1024,
             "_FIQ_STACK_SIZE":1024, "_UNDEF_STACK_SIZE":1024}
    for name, size in sizes.items():
        require(sym(name, True)["base"] == size, f"Changed reserved size {name}")
    heap = section(".heap")
    # ld puts ALIGN(16) *inside* these NOLOAD sections. Their headers may
    # therefore include up to 15 leading bytes; symbol spans are the actual
    # reservations. Do not mistake that legal alignment padding for heap use.
    heap_base = (heap["base"] + 15) & ~15
    heap_end = heap["base"] + heap["size"]
    require(0 <= heap_base - heap["base"] <= 15 and heap_end - heap_base == 8192,
            "Heap boundary/size changed")
    for name in ("_heap", "_heap_start", "HeapBase"):
        require(sym(name, True)["base"] == heap_base, "Heap start mismatch")
    for name in ("_heap_end", "HeapLimit"):
        require(sym(name, True)["base"] == heap_end, "Heap end mismatch")
    stack = section(".stack")
    stack_base = (stack["base"] + 15) & ~15
    require(stack["base"] == heap_end and 0 <= stack_base - stack["base"] <= 15 and
            stack["base"] + stack["size"] - stack_base == 0x3800,
            "Stack start/size mismatch")
    stack_parts = [("_stack_end", "_stack",8192), ("_irq_stack_end", "__irq_stack",1024),
                   ("_supervisor_stack_end", "__supervisor_stack",2048), ("_abort_stack_end", "__abort_stack",1024),
                   ("_fiq_stack_end", "__fiq_stack",1024), ("_undef_stack_end", "__undef_stack",1024)]
    cursor = stack_base
    for low, high, size in stack_parts:
        require(sym(low, True)["base"] == cursor and sym(high, True)["base"] == cursor + size, f"Stack partition mismatch {low}")
        cursor += size
    require(sym("__stack", True)["base"] == stack_base + 8192 and sym("_end", True)["base"] == cursor,
            "Stack aliases/_end mismatch")
    program_end = max(p["base"] + p["size"] for p in program)
    require(cursor == program_end and section(".bss")["base"] + section(".bss")["size"] <= heap["base"],
            "Program end/BSS/heap contract mismatch")
    for name, base, size in (("storage",0x06000000,0x400000), ("vector_history",0x06400000,0x400000),
                             ("trigger_history",0x06800000,0x800000)):
        s = sym(name)
        require(s["base"] == base and s["size"] == size, f"History storage symbol changed: {name}")
    if require_previous_plan:
        s = sym("previous_plan")
        bss = section(".bss")
        require(s["size"] == 130580 and s["section"] == bss["index"] and bss["base"] <= s["base"] and
                s["base"] + s["size"] <= bss["base"] + bss["size"], "previous_plan missing/not in BSS or wrong size")
    for name, origin, length in (("ps7_ddr_0",0x100000,0x700000), ("capture_history",0x6000000,0x800000),
                                 ("trigger_history",0x6800000,0x800000)):
        rows = re.findall(r"(?m)^" + name + r"\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s*$", map_text)
        require(len(rows) == 1 and tuple(int(x,16) for x in rows[0]) == (origin,length), f"MAP MEMORY mismatch: {name}")
    return dict(program_base=0x100000, program_end=program_end, program_bytes=program_end-0x100000,
                program_limit=0x800000, program_margin_bytes=0x800000-program_end,
                loads=loads, allocated_sections=alloc,
                heap=dict(base=heap_base, end=heap_end, bytes=8192, section_base=heap["base"], leading_padding_bytes=heap_base-heap["base"]),
                stacks=dict(base=stack_base, end=cursor, bytes=0x3800, main_bytes=8192,
                            section_base=stack["base"], leading_padding_bytes=stack_base-stack["base"]),
                previous_plan_bytes=130580 if require_previous_plan else None)


def numeric_defines(text):
    result = {}
    for name, expr in re.findall(r"(?m)^\s*#define\s+(\w+)\s+([^\r\n]+)", text):
        expr = expr.split("/*",1)[0].split("//",1)[0].strip()
        if name in result:
            continue
        expr = re.sub(r"\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b", r"\1", expr)
        try:
            node = ast.parse(expr, mode="eval")
            def value(n):
                if isinstance(n, ast.Expression): return value(n.body)
                if isinstance(n, ast.Constant) and type(n.value) is int: return n.value
                if isinstance(n, ast.Name): return result[n.id]
                if isinstance(n, ast.BinOp) and isinstance(n.op, (ast.Add, ast.Sub, ast.Mult)):
                    a, b = value(n.left), value(n.right)
                    return a+b if isinstance(n.op, ast.Add) else a-b if isinstance(n.op, ast.Sub) else a*b
                raise ValueError("not an integer macro")
            result[name] = value(node)
        except (SyntaxError, ValueError, KeyError):
            continue
    return result


def source_contract(source_root, bsp_include=None):
    root = Path(source_root)
    board = (root / "include/board_config.h").read_text(encoding="utf-8-sig")
    config = numeric_defines(board)
    expected = {"TRACEGPU_LCD_WIDTH":800, "TRACEGPU_LCD_HEIGHT":480, "TRACEGPU_FRAME_BYTES":1152000,
                "TRACEGPU_UI_CANVAS_BYTES":1536000, "TRACEGPU_FRAMEBUFFER_0":0x800000,
                "TRACEGPU_FRAMEBUFFER_1":0xA00000,"TRACEGPU_UI_CANVAS_0":0xC00000,"TRACEGPU_UI_CANVAS_1":0xE00000,
                "TRACEGPU_GPU_TEST_SRC":0x1800000,"TRACEGPU_GPU_TEST_DST":0x1900000,
                "PX_GPU_RUNTIME_START":0x2000000,"TRACEGPU_PAINTER_POOL_BYTES":0x4000000,
                "TRACEGPU_HISTORY_BASE":0x6000000,"TRACEGPU_HISTORY_LIMIT":0x6800000,
                "TRACEGPU_TRIGGER_HISTORY_BASE":0x6800000,"TRACEGPU_TRIGGER_HISTORY_LIMIT":0x7000000,
                "TRACEGPU_UART_DMA_BYTES":4096,"TRACEGPU_UART_DMA_BUFFER_0":0x8000000,
                "TRACEGPU_UART_DMA_BUFFER_1":0x8010000}
    for name, value in expected.items():
        require(config.get(name) == value, f"Board memory macro changed/unresolved: {name}")
    sg_path = root / "src/uart_capture_sg.c"
    sg_text = sg_path.read_text(encoding="utf-8-sig")
    sg = numeric_defines(sg_text)
    require(sg.get("BD_BASE") == 0x08100000 and sg.get("BD_COUNT") == 128 and sg.get("BUFFER_BASE") == 0x08000000,
            "SG fixed DDR layout changed")
    require("XAxiDma_BdRingCreate(ring,BD_BASE,BD_BASE,XAXIDMA_BD_MINIMUM_ALIGNMENT,BD_COUNT)" in re.sub(r"\s+", "", sg_text),
            "SG descriptor alignment binding changed")
    require("BUFFER_BASE+i*TRACEGPU_UART_DMA_BYTES" in re.sub(r"\s+", "", sg_text), "SG buffer stride changed")
    alignment = 64
    bsp_verified = False
    if bsp_include is not None:
        bsp_path = Path(bsp_include)
        hw = numeric_defines((bsp_path / "xaxidma_hw.h").read_text(encoding="utf-8-sig"))
        params = numeric_defines((bsp_path / "xparameters.h").read_text(encoding="utf-8-sig"))
        require(hw.get("XAXIDMA_BD_MINIMUM_ALIGNMENT") == 64, "BSP DMA descriptor alignment changed")
        require(params.get("XPAR_PS7_DDR_0_BASEADDRESS") == 0x100000 and
                params.get("XPAR_PS7_DDR_0_HIGHADDRESS") == 0x3fffffff, "BSP DDR range changed")
        bsp_verified = True
    ranges = [("legacy_rgb0",0x800000,1152000), ("legacy_rgb1",0xA00000,1152000),
              ("active_argb0",0xC00000,1536000), ("active_argb1",0xE00000,1536000),
              ("gpu_test_src_reserved_slot",0x1800000,0x100000), ("gpu_test_dst_reserved_slot",0x1900000,0x100000),
              ("painter_pool_reserved_not_allocated",0x2000000,0x4000000),
              ("capture_history",0x6000000,0x800000), ("trigger_history",0x6800000,0x800000),
              ("sg_payload_128x4096",0x8000000,128*4096), ("sg_descriptors_128x64",0x8100000,128*alignment)]
    spans = [(name, *interval(base,size,name)) for name,base,size in ranges]
    nonoverlap(spans)
    require(all(0x800000 <= start < end <= 0x40000000 for _,start,end in spans), "Reserved range outside DDR")
    linker = (root / "src/lscript.ld").read_text(encoding="utf-8-sig")
    for name, origin, length in (("ps7_ddr_0",0x100000,0x700000), ("capture_history",0x6000000,0x800000),
                                 ("trigger_history",0x6800000,0x800000)):
        m = re.search(r"\b"+name+r"\s*:\s*ORIGIN\s*=\s*(0x[\da-fA-F]+),\s*LENGTH\s*=\s*(0x[\da-fA-F]+)", linker)
        require(m and (int(m[1],16),int(m[2],16)) == (origin,length), f"Maintained linker region changed: {name}")
    return dict(bsp_verified=bsp_verified, dma_descriptor_alignment_bytes=alignment,
                reserved_ranges=[dict(name=n,base=a,end=b,bytes=b-a) for n,a,b in spans],
                scratch_slot_policy="Conservative 1MiB slots; startup memcpy uses 4KiB each",
                painter_pool_policy="64MiB reserved by board macros; no current allocation was inferred")


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify_memory(elf_path, map_path, source_root, *, require_previous_plan=True, bsp_include=None):
    """Return JSON-safe evidence, or raise MemoryContractError. Never writes."""
    elf_path, map_path, source_root = Path(elf_path), Path(map_path), Path(source_root)
    elf = parse_elf(elf_path.read_bytes())
    result = validate_elf_layout(elf, map_path.read_text(encoding="utf-8-sig"), require_previous_plan)
    result.update(source_contract(source_root,bsp_include))
    program_ranges = [("linked_program", result["program_base"], result["program_end"])]
    nonoverlap(program_ranges + [(x["name"],x["base"],x["end"]) for x in result["reserved_ranges"]])
    result.update(status="PASS", passed=True, elf=str(elf_path.resolve()), map=str(map_path.resolve()),
                  elf_sha256=sha(elf_path), map_sha256=sha(map_path),
                  source_contract_sha256={p:sha(source_root/p) for p in ("include/board_config.h","src/lscript.ld","src/uart_capture_sg.c")},
                  limits="Link-time boundary check only; runtime stack usage, coherency, and board validation remain untested")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf",required=True)
    parser.add_argument("--map",required=True)
    parser.add_argument("--source-root",required=True)
    parser.add_argument("--bsp-include")
    parser.add_argument("--allow-baseline-without-previous-plan",action="store_true")
    args = parser.parse_args()
    result = verify_memory(args.elf,args.map,args.source_root,require_previous_plan=not args.allow_baseline_without_previous_plan,bsp_include=args.bsp_include)
    print(json.dumps(result,indent=2,ensure_ascii=False))


if __name__ == "__main__":
    main()
