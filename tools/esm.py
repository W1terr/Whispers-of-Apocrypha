"""Skyrim SE plugin reader for research: records with group context, subrecords, localized strings."""
import struct
import zlib
from pathlib import Path

GAME = Path(r"D:\Games\steamapps\common\Skyrim Special Edition")
SCRATCH = Path(__file__).resolve().parent.parent / "scratch"


class Record:
    __slots__ = ("type", "flags", "formid", "body", "parents", "offset")

    def __init__(self, typ, flags, formid, body, parents, offset):
        self.type, self.flags, self.formid, self.body, self.parents, self.offset = typ, flags, formid, body, parents, offset

    def subs(self):
        pos, body, big = 0, self.body, None
        while pos + 6 <= len(body):
            typ = body[pos:pos + 4].decode(errors="replace")
            size = struct.unpack_from("<H", body, pos + 4)[0]
            if typ == "XXXX":
                big = struct.unpack_from("<I", body, pos + 6)[0]
                pos += 6 + size
                continue
            if big is not None:
                size, big = big, None
            yield typ, body[pos + 6:pos + 6 + size]
            pos += 6 + size

    def get(self, typ):
        for t, v in self.subs():
            if t == typ:
                return v
        return None

    def all(self, typ):
        return [v for t, v in self.subs() if t == typ]

    @property
    def edid(self):
        v = self.get("EDID")
        return v.rstrip(b"\0").decode(errors="replace") if v else ""


def walk(data):
    """Yields Record objects; parents = list of (group type, label) from outermost to innermost."""
    out = []
    stack = []  # (end offset, group type, label)
    pos = 0
    end = len(data)
    while pos < end:
        while stack and pos >= stack[-1][0]:
            stack.pop()
        typ = data[pos:pos + 4]
        size = struct.unpack_from("<I", data, pos + 4)[0]
        if typ == b"GRUP":
            label = struct.unpack_from("<I", data, pos + 8)[0]
            gtype = struct.unpack_from("<i", data, pos + 12)[0]
            stack.append((pos + size, gtype, label))
            pos += 24
            continue
        flags, formid = struct.unpack_from("<II", data, pos + 8)
        body = data[pos + 24:pos + 24 + size]
        if flags & 0x40000:
            body = zlib.decompress(body[4:])
        out.append(Record(typ.decode(), flags, formid, body, [(g, l) for _, g, l in stack], pos))
        pos += 24 + size
    return out


def load_strings(name="skyrim", lang="english"):
    table = {}
    for ext in ("strings", "dlstrings", "ilstrings"):
        p = SCRATCH / "bsa" / "strings" / f"{name}_{lang}.{ext}"
        data = p.read_bytes()
        count, _ = struct.unpack_from("<II", data, 0)
        base = 8 + count * 8
        for i in range(count):
            sid, off = struct.unpack_from("<II", data, 8 + i * 8)
            o = base + off
            if ext == "strings":
                e = data.index(b"\0", o)
                s = data[o:e]
            else:
                n = struct.unpack_from("<I", data, o)[0]
                s = data[o + 4:o + 4 + n].rstrip(b"\0")
            table[sid] = s.decode("cp1252", errors="replace")
    return table


class Plugin:
    def __init__(self, name="Skyrim.esm"):
        self.data = (GAME / "Data" / name).read_bytes()
        self.records = walk(self.data)
        self.by_id = {}
        for r in self.records:
            self.by_id.setdefault(r.formid, r)
        self.by_edid = {}
        for r in self.records:
            e = r.edid
            if e:
                self.by_edid.setdefault(e.lower(), r)
        self._strings = None

    @property
    def strings(self):
        if self._strings is None:
            self._strings = load_strings()
        return self._strings

    def lstr(self, value):
        if value is None:
            return ""
        return self.strings.get(struct.unpack_from("<I", value)[0], "?")

    def name(self, formid):
        r = self.by_id.get(formid)
        if not r:
            return f"{formid:08X}"
        return f"{r.edid or r.type}[{formid:08X}]"
