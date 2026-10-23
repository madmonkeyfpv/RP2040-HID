#!/usr/bin/env python3
"""Add MCP3208 + 4x 74HC165 + WAGO 250 terminals to the rp2040-basic EasyEDA
(Standard) schematic and PCB, leaving every existing part and track as is.

Usage: gen_easyeda.py SCH_IN PCB_IN OUT_DIR
Units: schematic = EasyEDA pixels (10 = one pin pitch); PCB = 10 mil.
"""
import copy
import itertools
import json
import os
import random
import re
import sys
import time

random.seed(2040)
SCH_IN, PCB_IN, OUT_DIR = sys.argv[1:4]
TS = int(time.time())

_ids = itertools.count(0x900000)


def gid():
    return "gge%d" % next(_ids)


def hexid(n):
    return "".join(random.choice("0123456789abcdef") for _ in range(n))


def f(v):
    return ("%.4f" % v).rstrip("0").rstrip(".")


# ---------------------------------------------------------------------------
# Stroke font for PCB silkscreen text (EasyEDA stores text as a vector path)
# Glyphs on a 4 x 6 grid, y down.
# ---------------------------------------------------------------------------
_O = [(1, 0), (3, 0), (4, 1), (4, 5), (3, 6), (1, 6), (0, 5), (0, 1), (1, 0)]
_P = [(0, 6), (0, 0), (3, 0), (4, 1), (4, 2), (3, 3), (0, 3)]
GLYPHS = {
    "0": [_O, [(4, 1), (0, 5)]],
    "1": [[(1, 1), (2, 0), (2, 6)], [(1, 6), (3, 6)]],
    "2": [[(0, 1), (1, 0), (3, 0), (4, 1), (4, 2), (0, 6), (4, 6)]],
    "3": [[(0, 0), (4, 0), (2, 2.5), (3, 2.5), (4, 3.5), (4, 5), (3, 6), (1, 6), (0, 5)]],
    "4": [[(3, 6), (3, 0), (0, 4), (4, 4)]],
    "5": [[(4, 0), (0, 0), (0, 2.5), (3, 2.5), (4, 3.5), (4, 5), (3, 6), (0, 6)]],
    "6": [[(3, 0), (1, 0), (0, 1), (0, 5), (1, 6), (3, 6), (4, 5), (4, 3.5), (3, 2.5), (0, 2.5)]],
    "7": [[(0, 0), (4, 0), (1.5, 6)]],
    "8": [[(1, 3), (0, 2), (0, 1), (1, 0), (3, 0), (4, 1), (4, 2), (3, 3), (1, 3), (0, 4),
           (0, 5), (1, 6), (3, 6), (4, 5), (4, 4), (3, 3)]],
    "9": [[(4, 3.5), (1, 3.5), (0, 2.5), (0, 1), (1, 0), (3, 0), (4, 1), (4, 5), (3, 6), (1, 6)]],
    "A": [[(0, 6), (2, 0), (4, 6)], [(0.7, 4), (3.3, 4)]],
    "B": [[(0, 3), (3, 3), (4, 4), (4, 5), (3, 6), (0, 6), (0, 0), (3, 0), (4, 1), (4, 2), (3, 3)]],
    "C": [[(4, 1), (3, 0), (1, 0), (0, 1), (0, 5), (1, 6), (3, 6), (4, 5)]],
    "D": [[(0, 0), (0, 6), (2.5, 6), (4, 4.5), (4, 1.5), (2.5, 0), (0, 0)]],
    "E": [[(4, 0), (0, 0), (0, 6), (4, 6)], [(0, 3), (3, 3)]],
    "F": [[(4, 0), (0, 0), (0, 6)], [(0, 3), (3, 3)]],
    "G": [[(4, 1), (3, 0), (1, 0), (0, 1), (0, 5), (1, 6), (3, 6), (4, 5), (4, 3.5), (2, 3.5)]],
    "H": [[(0, 0), (0, 6)], [(4, 0), (4, 6)], [(0, 3), (4, 3)]],
    "I": [[(1, 0), (3, 0)], [(2, 0), (2, 6)], [(1, 6), (3, 6)]],
    "J": [[(4, 0), (4, 5), (3, 6), (1, 6), (0, 5)]],
    "K": [[(0, 0), (0, 6)], [(4, 0), (0, 3.5)], [(1.2, 2.8), (4, 6)]],
    "L": [[(0, 0), (0, 6), (4, 6)]],
    "M": [[(0, 6), (0, 0), (2, 3), (4, 0), (4, 6)]],
    "N": [[(0, 6), (0, 0), (4, 6), (4, 0)]],
    "O": [_O],
    "P": [_P],
    "Q": [_O, [(2.5, 4.5), (4, 6)]],
    "R": [_P, [(2, 3), (4, 6)]],
    "S": [[(4, 1), (3, 0), (1, 0), (0, 1), (0, 2), (1, 3), (3, 3), (4, 4), (4, 5), (3, 6), (1, 6), (0, 5)]],
    "T": [[(0, 0), (4, 0)], [(2, 0), (2, 6)]],
    "U": [[(0, 0), (0, 5), (1, 6), (3, 6), (4, 5), (4, 0)]],
    "V": [[(0, 0), (2, 6), (4, 0)]],
    "W": [[(0, 0), (1, 6), (2, 3), (3, 6), (4, 0)]],
    "X": [[(0, 0), (4, 6)], [(4, 0), (0, 6)]],
    "Y": [[(0, 0), (2, 3), (4, 0)], [(2, 3), (2, 6)]],
    "Z": [[(0, 0), (4, 0), (0, 6), (4, 6)]],
    "k": [[(0, 0), (0, 6)], [(3.5, 2.5), (0, 4.5)], [(1.2, 3.8), (3.5, 6)]],
    "n": [[(0, 2.5), (0, 6)], [(0, 3.5), (1, 2.5), (3, 2.5), (4, 3.5), (4, 6)]],
    "u": [[(0, 2.5), (0, 5), (1, 6), (3, 6), (4, 5)], [(4, 2.5), (4, 6)]],
    "x": [[(0, 2.5), (4, 6)], [(4, 2.5), (0, 6)]],
    "-": [[(0.5, 3), (3.5, 3)]],
    "+": [[(0.5, 3), (3.5, 3)], [(2, 1.5), (2, 4.5)]],
    "/": [[(0, 6), (4, 0)]],
    ".": [[(1.8, 5.6), (2.2, 5.6), (2.2, 6), (1.8, 6), (1.8, 5.6)]],
    " ": [],
}


def text_metrics(text, size):
    s = size * 0.9 / 6.0
    return s, len(text) * 6 * s - 2 * s, 6 * s  # scale, width, height


def text_path(text, x, y, size):
    """Path for text whose left baseline is at (x, y)."""
    s, _, h = text_metrics(text, size)
    out = []
    for i, ch in enumerate(text):
        for stroke in GLYPHS.get(ch, GLYPHS.get(ch.upper(), [])):
            pts = [(x + (i * 6 + px) * s, y - h + py * s) for px, py in stroke]
            out.append("M %s %s " % (f(pts[0][0]), f(pts[0][1])) +
                       " ".join("L %s %s" % (f(a), f(b)) for a, b in pts[1:]))
    return " ".join(out)


def pcb_text(kind, text, x, y, size=4.5, hidden=False, center=False):
    if center:
        _, w, h = text_metrics(text, size)
        x, y = x - w / 2, y + h / 2
    return "TEXT~%s~%s~%s~0.6~0~0~3~~%s~%s~%s~%s~%s~~0~" % (
        kind, f(x), f(y), f(size), text, text_path(text, x, y, size),
        "none" if hidden else "", gid())


# ---------------------------------------------------------------------------
# PCB primitives
# ---------------------------------------------------------------------------
def rect_pts(cx, cy, w, h):
    return " ".join(f(v) for v in (cx - w / 2, cy - h / 2, cx + w / 2, cy - h / 2,
                                   cx + w / 2, cy + h / 2, cx - w / 2, cy + h / 2))


def smd_pad(cx, cy, w, h, net, num):
    return "PAD~RECT~%s~%s~%s~%s~1~%s~%s~0~%s~0~%s~0~~Y~0~0~0.2~%s,%s" % (
        f(cx), f(cy), f(w), f(h), net, num, rect_pts(cx, cy, w, h), gid(), f(cx), f(cy))


def th_pad(cx, cy, d, hole_r, net, num, square=False, plated="Y"):
    if square:
        return "PAD~RECT~%s~%s~%s~%s~11~%s~%s~%s~%s~0~%s~0~~%s~0~0~0.2~%s,%s" % (
            f(cx), f(cy), f(d), f(d), net, num, f(hole_r), rect_pts(cx, cy, d, d), gid(),
            plated, f(cx), f(cy))
    return "PAD~ELLIPSE~%s~%s~%s~%s~11~%s~%s~%s~~0~%s~0~~%s~0~0~0.2~%s,%s" % (
        f(cx), f(cy), f(d), f(d), net, num, f(hole_r), gid(), plated, f(cx), f(cy))


def silk(*pts, w=0.8):
    return "TRACK~%s~3~~%s~%s~0" % (f(w), " ".join(f(v) for p in pts for v in p), gid())


def silk_circle(cx, cy, r, w=0.6):
    return "CIRCLE~%s~%s~%s~%s~3~%s~0~~" % (f(cx), f(cy), f(r), f(w), gid())


class Part:
    """One component: schematic symbol + PCB footprint sharing an ID."""

    def __init__(self, ref, value, package, sch_para, pcb_para, prefix):
        self.ref, self.value, self.package = ref, value, package
        self.sch_para, self.pcb_para, self.prefix = sch_para, pcb_para, prefix
        self.gid = "gge" + hexid(16)
        self.uuid = hexid(32)
        self.pads = {}  # pad number -> net


def para(pairs):
    return "`".join("%s`%s" % kv for kv in pairs) + "`"


def pcb_lib(part, x, y, rot, prims):
    hdr = "LIB~%s~%s~%s~%s~~%s~1~%s~%d~0~~yes~~" % (
        f(x), f(y), part.pcb_para, rot, part.gid, part.uuid, TS)
    return "#@$".join([hdr] + prims)


# --- footprints ------------------------------------------------------------
def fp_soic16(part, cx, cy, nets):
    prims = [pcb_text("P", part.ref, cx - 8, cy - 23.5), pcb_text("N", part.value, cx - 10, cy + 27, hidden=True)]
    for n in range(1, 17):
        if n <= 8:
            x, y = cx - 10.63, cy - 17.5 + 5 * (n - 1)
        else:
            x, y = cx + 10.63, cy + 17.5 - 5 * (n - 9)
        prims.append(smd_pad(x, y, 6.1, 2.36, nets[n], n))
        part.pads[n] = nets[n]
    prims += [silk((cx - 6.6, cy - 20.3), (cx + 6.6, cy - 20.3)),
              silk((cx - 6.6, cy + 20.3), (cx + 6.6, cy + 20.3)),
              silk_circle(cx - 14.8, cy - 21.2, 0.8)]
    return pcb_lib(part, cx, cy, "", prims)


def fp_rn4(part, cx, cy, nets):
    """10k x4 convex array, 0603x4 (3.2 x 1.6 mm). Pins 1-4 bottom row L->R,
    5-8 top row R->L; resistors are 1-8, 2-7, 3-6, 4-5."""
    prims = [pcb_text("P", part.ref, cx - 6, cy - 6.5, size=3.5), pcb_text("N", part.value, cx, cy + 9, hidden=True)]
    xs = [-4.72, -1.57, 1.57, 4.72]
    for i in range(4):
        prims.append(smd_pad(cx + xs[i], cy + 3.0, 1.97, 3.0, nets[i + 1], i + 1))
        prims.append(smd_pad(cx + xs[3 - i], cy - 3.0, 1.97, 3.0, nets[i + 5], i + 5))
    part.pads.update(nets)
    prims.append(silk_circle(cx - 7.4, cy + 5.5, 0.6))
    return pcb_lib(part, cx, cy, "", prims)


def fp_c0402(part, cx, cy, nets):
    prims = [pcb_text("P", part.ref, cx - 4.5, cy - 3, size=3.3465),
             pcb_text("N", part.value, cx, cy + 6, hidden=True),
             smd_pad(cx - 1.704, cy, 2.227, 2.126, nets[1], 1),
             smd_pad(cx + 1.704, cy, 2.227, 2.126, nets[2], 2)]
    part.pads.update(nets)
    return pcb_lib(part, cx, cy, "", prims)


# WAGO 250-1xx: 3.5 mm pitch, one in-line solder pin per pole, 12 mm deep,
# body n * 3.5 + 1.5 mm wide, 45 degree conductor entry over the front edge.
W250_PITCH = 13.7795      # 3.5 mm
W250_FRONT = 13.7795      # pin row to front (wire entry) edge, 3.5 mm
W250_REAR = 33.4646       # pin row to rear edge, 8.5 mm
W250_END = 2.9528         # body overhang past the outer half-pitch, 0.75 mm


def wago250_length(n):
    return n * W250_PITCH + 2 * W250_END


def fp_wago250(part, p1, step, front, nets, labels):
    """p1 = pole 1 pin, step = unit vector to the next pole, front = unit
    vector towards the wire entry (board edge)."""
    n = len(nets)
    sx, sy = step
    fx, fy = front
    rot = {(0, 1): "", (-1, 0): "90", (0, -1): "180", (1, 0): "270"}[front]

    def at(along, across):
        return (p1[0] + sx * along + fx * across, p1[1] + sy * along + fy * across)

    prims = []
    for i in range(n):
        x, y = at(W250_PITCH * i, 0)
        prims.append(th_pad(x, y, 7.0866, 1.9685, nets[i], i + 1, square=(i == 0)))
        part.pads[i + 1] = nets[i]
        # Pole labels sit just behind the body. Edge-on strips (top/bottom) are
        # limited to the pitch; side strips can use full names.
        side = front[1] == 0
        size = 3.6 if side or len(labels[i]) <= 3 else 3.0
        _, w, _ = text_metrics(labels[i], size)
        lx, ly = at(W250_PITCH * i, -(W250_REAR + 3 + w / 2) if side else -(W250_REAR + 5))
        prims.append(pcb_text("L", labels[i], lx, ly, size=size, center=True))
    half = W250_PITCH / 2 + W250_END
    last = W250_PITCH * (n - 1)
    a, b = at(-half, -W250_REAR), at(last + half, -W250_REAR)
    c, d = at(last + half, W250_FRONT), at(-half, W250_FRONT)
    prims.append(silk(a, b, c, d, a))
    # push-button marks, one per pole, on the rear half of the body
    for i in range(n):
        prims.append(silk(at(W250_PITCH * i - 3, -20), at(W250_PITCH * i + 3, -20),
                          at(W250_PITCH * i + 3, -12), at(W250_PITCH * i - 3, -12),
                          at(W250_PITCH * i - 3, -20), w=0.6))
    tx, ty = at(last + half + 7, -W250_REAR / 2)   # just past the last pole
    prims.insert(0, pcb_text("P", part.ref, tx, ty, size=4.5, center=True))
    cx, cy = at(last / 2, 0)
    prims.insert(1, pcb_text("N", part.value, cx, cy, hidden=True, center=True))
    return pcb_lib(part, cx, cy, rot, prims)


# ---------------------------------------------------------------------------
# Schematic primitives
# ---------------------------------------------------------------------------
def sch_pin(num, name, x, y, side, show_name=True):
    """side 'L': pin end at (x, y), body to the right. 'R': body to the left."""
    i = gid()
    if side == "L":
        return ("P~show~0~%s~%s~%s~180~%s~0^^%s~%s^^M %s %s h 10~#880000^^%d~%s~%s~0~%s~start~~~#0000FF"
                "^^1~%s~%s~0~%s~end~~~#0000FF^^0~%s~%s^^0~M %s %s L %s %s L %s %s") % (
            num, x, y, i, x, y, x, y, 1 if show_name else 0, x + 13, y + 3.5, name,
            x + 9, y - 1, num, x + 7, y, x + 10, y + 3, x + 13, y, x + 10, y - 3)
    return ("P~show~0~%s~%s~%s~0~%s~0^^%s~%s^^M %s %s h -10~#880000^^%d~%s~%s~0~%s~end~~~#0000FF"
            "^^1~%s~%s~0~%s~start~~~#0000FF^^0~%s~%s^^0~M %s %s L %s %s L %s %s") % (
        num, x, y, i, x, y, x, y, 1 if show_name else 0, x - 13, y + 3.5, name,
        x - 9, y - 1, num, x - 7, y, x - 10, y - 3, x - 13, y, x - 10, y + 3)


def sch_text(kind, x, y, text):
    return "T~%s~%s~%s~0~#000080~Arial~~~~~comment~%s~1~start~%s~0~pinpart" % (kind, x, y, text, gid())


def sch_lib(part, x, y, prims):
    hdr = "LIB~%s~%s~%s~~0~%s~%s~%s~0~%s~yes~yes~%s~%d~%s" % (
        x, y, part.sch_para, part.gid, part.uuid, hexid(32), hexid(16), hexid(32), TS, hexid(32))
    return "#@$".join([hdr] + prims)


def net_label(x, y, name, side):
    """Net label touching (x, y). side 'L' extends left, 'R' extends right."""
    i, j = gid(), gid()
    if side == "L":
        return ("F~part_netLabel_netPort~%s~%s~0~%s~~0^^%s~%s^^%s~#0000FF~%s~%s~0~end~1~Times New Roman~8pt~flag_%s"
                "^^PL~%s %s %s %s %s %s %s %s %s %s %s %s~#0000FF~1~0~transparent~%s~0") % (
            x, y, i, x, y, name, x - 22, y + 3.5, i, x, y, x - 5, y + 5, x - 20, y + 5, x - 20, y - 5,
            x - 5, y - 5, x, y, j)
    return ("F~part_netLabel_netPort~%s~%s~180~%s~~0^^%s~%s^^%s~#0000FF~%s~%s~0~start~1~Times New Roman~8pt~flag_%s"
            "^^PL~%s %s %s %s %s %s %s %s %s %s %s %s~#0000FF~1~0~transparent~%s~0") % (
        x, y, i, x, y, name, x + 22, y + 3.5, i, x, y, x + 5, y - 5, x + 20, y - 5, x + 20, y + 5,
        x + 5, y + 5, x, y, j)


def no_connect(x, y):
    return "O~%s~%s~%s~M %s %s L %s %s M %s %s L %s %s~#33cc33~0" % (
        x, y, gid(), x - 4, y - 4, x + 4, y + 4, x + 4, y - 4, x - 4, y + 4)


SCH_NETS = []  # (part ref, pin number, net) for the cross-check


def symbol(part, x, y, w, left, right, extra=(), show_names=True):
    """Box symbol at top-left (x, y); left/right = [(num, name, net)].
    net None = no-connect. Returns shapes (symbol + labels)."""
    rows = max(len(left), len(right))
    h = rows * 10 + 10
    prims = [sch_text("N", x, y + h + 10, part.value), sch_text("P", x, y - 5, part.ref),
             "R~%s~%s~~~%s~%s~#880000~1~0~none~%s~0~" % (x, y, w, h, gid())]
    out = []
    for i, (num, name, net) in enumerate(left):
        py = y + 10 + 10 * i
        prims.append(sch_pin(num, name, x - 10, py, "L", show_names))
        out.append(net_label(x - 10, py, net, "L") if net else no_connect(x - 10, py))
        SCH_NETS.append((part.ref, num, net or ""))
    for i, (num, name, net) in enumerate(right):
        py = y + 10 + 10 * i
        prims.append(sch_pin(num, name, x + w + 10, py, "R", show_names))
        out.append(net_label(x + w + 10, py, net, "R") if net else no_connect(x + w + 10, py))
        SCH_NETS.append((part.ref, num, net or ""))
    prims += list(extra)
    return [sch_lib(part, x, y, prims)] + out


def sch_pl(*pts):
    return "PL~%s~#880000~1~0~none~%s~0" % (" ".join("%s %s" % p for p in pts), gid())


# ---------------------------------------------------------------------------
# Part definitions
# ---------------------------------------------------------------------------
GND, V33 = "GND", "+3V3"
C100N = ("CL05B104KO5NNNC", "C1525", "SAMSUNG(三星)")
C1U = ("CL05A105KA5NQNC", "C52923", "SAMSUNG")


def mk_cap(ref, val, mpn):
    m, lcsc, mfr = mpn
    sp = para([("package", "C0402"), ("nameAlias", "Capacitance"), ("Supplier", "LCSC"), ("Manufacturer", mfr),
               ("Manufacturer Part", m), ("Supplier Part", lcsc), ("JLCPCB Part Class", "Basic Part"),
               ("spicePre", "C"), ("spiceSymbolName", m)])
    pp = para([("package", "C0402"), ("BOM_Supplier", "LCSC"), ("BOM_Manufacturer", mfr),
               ("BOM_Manufacturer Part", m), ("nameAlias", "Capacitance"), ("BOM_Supplier Part", lcsc),
               ("BOM_JLCPCB Part Class", "Basic Part"), ("spicePre", "C"), ("spiceSymbolName", m)])
    return Part(ref, val, "C0402", sp, pp, "C")


def mk_ic(ref, val, mfr, mpn, pkg):
    sp = para([("package", pkg), ("Manufacturer", mfr), ("Manufacturer Part", mpn), ("Supplier", "LCSC"),
               ("Supplier Part", ""), ("spicePre", "U"), ("spiceSymbolName", val)])
    pp = para([("package", pkg), ("BOM_Manufacturer", mfr), ("BOM_Manufacturer Part", mpn),
               ("BOM_Supplier", "LCSC"), ("BOM_Supplier Part", ""), ("spicePre", "U"), ("spiceSymbolName", val)])
    return Part(ref, val, pkg, sp, pp, "U")


def mk_rn(ref):
    pkg = "RES-ARRAY-SMD_0603-8P-L3.2-W1.6-BL"
    sp = para([("package", pkg), ("Manufacturer", "UniOhm"), ("Manufacturer Part", "4D03WGJ0103T5E"),
               ("Resistance", "10k x4"), ("Supplier", "LCSC"), ("Supplier Part", ""), ("spicePre", "RN"),
               ("spiceSymbolName", "10k x4")])
    pp = para([("package", pkg), ("BOM_Manufacturer", "UniOhm"), ("BOM_Manufacturer Part", "4D03WGJ0103T5E"),
               ("BOM_Supplier", "LCSC"), ("BOM_Supplier Part", ""), ("spicePre", "RN"),
               ("spiceSymbolName", "10k x4")])
    return Part(ref, "10k x4", pkg, sp, pp, "RN")


def mk_wago(ref, poles):
    mpn = "250-1%02d" % poles
    pkg = "WAGO-250-1%02d_P3.50" % poles
    sp = para([("package", pkg), ("Manufacturer", "WAGO"), ("Manufacturer Part", mpn), ("Supplier", ""),
               ("Supplier Part", ""), ("spicePre", "J"), ("spiceSymbolName", mpn)])
    pp = para([("package", pkg), ("BOM_Manufacturer", "WAGO"), ("BOM_Manufacturer Part", mpn),
               ("BOM_Supplier", ""), ("BOM_Supplier Part", ""), ("spicePre", "J"), ("spiceSymbolName", mpn)])
    return Part(ref, mpn, pkg, sp, pp, "J")


SOIC16 = "SOIC-16_L9.9-W3.9-P1.27-LS6.0-BL"

# ---------------------------------------------------------------------------
# Board and placement (PCB units). The board grows left and down only; the
# RP2040 block, USB-B (top right) and the original top/right edges stay put.
# ---------------------------------------------------------------------------
X0, X1, Y0, Y1 = 3710, 4261, 3059.5, 3512.5        # ~140 x 115 mm
EDGE = 2 + W250_FRONT                               # board edge to pin row
BOTTOM_PINS, TOP_PINS = Y1 - EDGE, Y0 + EDGE
LEFT_PINS, RIGHT_PINS = X0 + EDGE, X1 - EDGE


def first_pole(start):
    """Pole-1 position for a strip whose body starts at `start`."""
    return start + W250_PITCH / 2 + W250_END


L10 = wago250_length(10)
# bottom: J3 J4 J5, 4 mm apart, from 11 mm in from the left edge
BOT_START = [X0 + 44 + k * (L10 + 16) for k in range(3)]
WAGO_BTN = [((first_pole(BOT_START[k]), BOTTOM_PINS), (1, 0), (0, 1)) for k in range(3)]
WAGO_BTN.append(((RIGHT_PINS, first_pole(3200)), (0, 1), (1, 0)))      # J6, right edge
WAGO_AX = [((LEFT_PINS, first_pole(3100)), (0, 1), (-1, 0)),             # J1
           ((LEFT_PINS, first_pole(3100 + wago250_length(12) + 10)), (0, 1), (-1, 0))]  # J2
WAGO_J7 = ((first_pole(3770), TOP_PINS), (1, 0), (0, -1))
WAGO_J8 = ((first_pole(3770 + L10 + 20), TOP_PINS), (1, 0), (0, -1))
WAGO_J9 = ((RIGHT_PINS, first_pole(3200 + L10 + 14)), (0, 1), (1, 0))

BOT_CENTRE = [first_pole(BOT_START[k]) + 4.5 * W250_PITCH for k in range(3)]
SR_X = [BOT_CENTRE[0], BOT_CENTRE[1], BOT_CENTRE[2], (BOT_CENTRE[1] + BOT_CENTRE[2]) / 2]
SR_Y = 3395
RN_Y = 3440
RN_POS = [(x + dx, RN_Y) for x in SR_X for dx in (-13, 13)]
U9_POS = (3810, 3280)
AXIS_NAMES = ["X", "Y", "Z", "RX", "RY", "RZ", "S1", "S2"]

# RP2040 pad number -> new net (pads 2-9 GP0-7, 11-18 GP8-15, 27-34 GP16-22, 38-40 GP26-28)
RP_NETS = {}
for g in range(8):
    RP_NETS[2 + g] = "GP%d" % g
for g in range(8, 16):
    RP_NETS[3 + g] = "GP%d" % g
RP_NETS.update({27: "ADC_MISO", 28: "ADC_CS", 29: "ADC_SCK", 30: "ADC_MOSI",
                31: "SR_DATA", 32: "SR_CLK", 34: "SR_LOAD", 38: "GP26", 39: "GP27", 40: "GP28"})
# matching schematic pin y on sheet 1 (pins at x = 915)
RP_SCH_Y = {**{2 + g: -480 + 10 * g for g in range(8)}, **{3 + g: -480 + 10 * g for g in range(8, 16)},
            27: -320, 28: -310, 29: -300, 30: -290, 31: -280, 32: -270, 34: -260, 38: -210, 39: -200, 40: -190}

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
sch = json.load(open(SCH_IN))
pcb = json.load(open(PCB_IN))
sheet1 = sch["schematics"][0]["dataStr"]["shape"]

# Sheet 1: label the RP2040 GPIOs
for pad, net in RP_NETS.items():
    sheet1.append(net_label(915, RP_SCH_Y[pad], net, "R"))
sheet1.append("T~L~935~-160~0~#0000FF~~9pt~~~~comment~GP16-22: MCP3208 + 74HC165 (sheet 2). "
              "Spare GPIOs go to WAGO J7-J9.~1~start~%s~0~pinpart" % gid())

# Sheet 2
s2 = []
frame = [x for x in sheet1 if "frame_lib" in x][0]
s2.append(re.sub(r"gge\w+", lambda m: gid(), frame))
s2.append("T~L~60~-1090~0~#0000FF~~24pt~~~~comment~IO expansion: MCP3208 (8 axes), 4x 74HC165 (32 buttons), "
          "WAGO 250 terminals~1~start~%s~0~pinpart" % gid())
s2.append("T~L~60~-1065~0~#0000FF~~9pt~~~~comment~Firmware: build with MCP3208_ENABLED 1 and SHIFT_REG_ENABLED 1 "
          "(default Pico pins). Chip 1 (U10) is nearest the RP2040: its A-H are buttons 1-8.~1~start~%s~0~pinpart" % gid())

pcb_add = []
parts = []

# --- MCP3208 -----------------------------------------------------------------
u9 = mk_ic("U9", "MCP3208", "Microchip", "MCP3208-CI/SL", SOIC16)
parts.append(u9)
mcp_nets = {i + 1: "AX%d" % (i + 1) for i in range(8)}
mcp_nets.update({9: GND, 10: "ADC_CS", 11: "ADC_MOSI", 12: "ADC_MISO", 13: "ADC_SCK", 14: GND, 15: V33, 16: V33})
s2 += symbol(u9, 140, -1000, 70,
             [(i + 1, "CH%d" % i, mcp_nets[i + 1]) for i in range(8)],
             [(16, "VDD", V33), (15, "VREF", V33), (14, "AGND", GND), (13, "CLK", "ADC_SCK"),
              (12, "DOUT", "ADC_MISO"), (11, "DIN", "ADC_MOSI"), (10, "CS", "ADC_CS"), (9, "DGND", GND)])
pcb_add.append(fp_soic16(u9, U9_POS[0], U9_POS[1], mcp_nets))

caps = []


def cap(ref, val, mpn, sx, sy, px, py):
    c = mk_cap(ref, val, mpn)
    parts.append(c)
    plates = [sch_pl((sx + 18, sy - 6), (sx + 18, sy + 6)), sch_pl((sx + 22, sy - 6), (sx + 22, sy + 6)),
              sch_pl((sx + 10, sy), (sx + 18, sy)), sch_pl((sx + 22, sy), (sx + 30, sy))]
    prims = [sch_text("N", sx + 8, sy + 18, val), sch_text("P", sx + 8, sy - 10, ref),
             sch_pin(1, "1", sx, sy, "L", False), sch_pin(2, "2", sx + 40, sy, "R", False)] + plates
    s2.append(sch_lib(c, sx, sy, prims))
    s2.append(net_label(sx, sy, V33, "L"))
    s2.append(net_label(sx + 40, sy, GND, "R"))
    SCH_NETS.extend([(ref, 1, V33), (ref, 2, GND)])
    pcb_add.append(fp_c0402(c, px, py, {1: V33, 2: GND}))


cap("PC17", "100nF", C100N, 360, -1000, U9_POS[0] + 23, U9_POS[1] - 22)
cap("PC22", "1uF", C1U, 360, -955, U9_POS[0] + 23, U9_POS[1] - 29)
s2.append("T~L~330~-1020~0~#0000FF~~9pt~~~~comment~Place next to U9 pins 15/16~1~start~%s~0~pinpart" % gid())

# --- WAGO axis terminals J1, J2 ------------------------------------------
def wago(ref, nets, labels, sx, sy, p1, step, front, title):
    j = mk_wago(ref, len(nets))
    parts.append(j)
    s2.extend(symbol(j, sx, sy, 50, [(i + 1, labels[i], nets[i]) for i in range(len(nets))], []))
    s2.append("T~L~%s~%s~0~#0000FF~~9pt~~~~comment~%s~1~start~%s~0~pinpart" % (sx - 40, sy - 18, title, gid()))
    pcb_add.append(fp_wago250(j, p1, step, front, nets, labels))


for k in range(2):
    nets, labels = [], []
    for a in range(4 * k, 4 * k + 4):
        nets += [V33, "AX%d" % (a + 1), GND]
        labels += ["3V3", AXIS_NAMES[a], "GND"]
    wago("J%d" % (k + 1), nets, labels, 560 + 150 * k, -1010, *WAGO_AX[k],
         "Axes %d-%d (+3V3 / signal / GND)" % (4 * k + 1, 4 * k + 4))

# --- 74HC165 chain -------------------------------------------------------------
for k in range(4):
    ref = "U%d" % (10 + k)
    u = mk_ic(ref, "74HC165", "Nexperia", "74HC165D,653", SOIC16)
    parts.append(u)
    b0 = 8 * k
    btn = ["BTN%d" % (b0 + i + 1) for i in range(8)]
    qh = "SR_DATA" if k == 0 else "SR_Q%d" % (k + 1)
    ser = V33 if k == 3 else "SR_Q%d" % (k + 2)
    nets = {11: btn[0], 12: btn[1], 13: btn[2], 14: btn[3], 3: btn[4], 4: btn[5], 5: btn[6], 6: btn[7],
            16: V33, 10: ser, 2: "SR_CLK", 1: "SR_LOAD", 15: GND, 9: qh, 7: "", 8: GND}
    col, row = k % 2, k // 2
    sx, sy = 160 + 600 * col, -800 + 300 * row
    s2 += symbol(u, sx, sy, 70,
                 [(11, "A", btn[0]), (12, "B", btn[1]), (13, "C", btn[2]), (14, "D", btn[3]),
                  (3, "E", btn[4]), (4, "F", btn[5]), (5, "G", btn[6]), (6, "H", btn[7])],
                 [(16, "VCC", V33), (10, "SER", ser), (2, "CLK", "SR_CLK"), (1, "SH/LD", "SR_LOAD"),
                  (15, "CLK INH", GND), (9, "QH", qh), (7, "QH'", None), (8, "GND", GND)])
    pcb_add.append(fp_soic16(u, SR_X[k], SR_Y, nets))

    # pull-up arrays
    for h in range(2):
        rn = mk_rn("RN%d" % (2 * k + h + 1))
        parts.append(rn)
        ins = btn[4 * h:4 * h + 4]
        rnets = {1: ins[0], 2: ins[1], 3: ins[2], 4: ins[3], 5: V33, 6: V33, 7: V33, 8: V33}
        rx, ry = sx + 230, sy + 75 * h
        extra = []
        for i in range(4):
            py = ry + 10 + 10 * i
            extra += ["R~%s~%s~~~16~6~#880000~1~0~none~%s~0~" % (rx + 12, py - 3, gid()),
                      sch_pl((rx, py), (rx + 12, py)), sch_pl((rx + 28, py), (rx + 40, py))]
        s2 += symbol(rn, rx, ry, 40, [(i + 1, "", ins[i]) for i in range(4)],
                     [(8 - i, "", V33) for i in range(4)], extra, show_names=False)
        pcb_add.append(fp_rn4(rn, RN_POS[2 * k + h][0], RN_POS[2 * k + h][1], rnets))

    cap("PC%d" % (18 + k), "100nF", C100N, sx + 230, sy + 170, SR_X[k] + 11, SR_Y - 26)

    jnets = btn + [GND, GND]
    jlab = [str(b0 + i + 1) for i in range(8)] + (["GND", "GND"] if k == 3 else ["G", "G"])
    p1, step, front = WAGO_BTN[k]
    wago("J%d" % (3 + k), jnets, jlab, sx + 400, sy, p1, step, front, "Buttons %d-%d" % (b0 + 1, b0 + 8))

# --- spare GPIO terminals --------------------------------------------------
wago("J7", ["GP%d" % i for i in range(8)] + [GND, GND], ["P%d" % i for i in range(8)] + ["G", "G"],
     1400, -1010, *WAGO_J7, "Spare GP0-7 (hat / encoders)")
wago("J8", ["GP%d" % i for i in range(8, 16)] + [GND, GND], ["P%d" % i for i in range(8, 16)] + ["G", "G"],
     1400, -830, *WAGO_J8, "Spare GP8-15")
wago("J9", [V33, "GP26", "GP27", "GP28", GND, GND], ["3V3", "GP26", "GP27", "GP28", "GND", "GND"],
     1400, -650, *WAGO_J9, "Spare GP26-28")

# ---------------------------------------------------------------------------
# PCB: RP2040 pad nets, board outline, holes, pours
# ---------------------------------------------------------------------------
shapes = pcb["shape"]
for idx, s in enumerate(shapes):
    if s.startswith("LIB") and "`RP2040`" in s.split("#@$")[0]:
        prims = s.split("#@$")
        for j, pr in enumerate(prims):
            if pr.startswith("PAD~"):
                fl = pr.split("~")
                if fl[8].isdigit() and int(fl[8]) in RP_NETS:
                    fl[7] = RP_NETS[int(fl[8])]
                    prims[j] = "~".join(fl)
        shapes[idx] = "#@$".join(prims)

for idx, s in enumerate(shapes):
    if s.startswith("COPPERAREA"):
        fl = s.split("~")
        if fl[2] == "2":   # bottom: whole board
            fl[4] = "M %s %s L %s %s L %s %s L %s %s Z" % (X0 + 1, Y0 + 1, X1 - 1, Y0 + 1, X1 - 1, Y1 - 1, X0 + 1, Y1 - 1)
        else:              # top: keep the original keep-back near the USB socket
            fl[4] = ("M %s %s L 4088 %s L 4088 3103 L 4252.5 3102.5 L 4254 3318.5 L %s 3318.5 L %s %s "
                     "L %s %s Z") % (X0 + 1, Y0 + 1, Y0 + 1, X1 - 1, X1 - 1, Y1 - 1, X0 + 1, Y1 - 1)
        fl[10] = "[]"     # fill is rebuilt in EasyEDA (Shift+B)
        shapes[idx] = "~".join(fl)

shapes.append("TRACK~1~10~~%s %s %s %s %s %s %s %s %s %s~%s~0" % (X0, Y0, X1, Y0, X1, Y1, X0, Y1, X0, Y0, gid()))
for hx, hy in [(X0 + 16, Y0 + 16), (X0 + 16, Y1 - 16), (X1 - 16, Y1 - 16)]:
    shapes.append("HOLE~%s~%s~6.2992~%s~0" % (hx, hy, gid()))
shapes.append(pcb_text("L", "RP2040-HID", 3880, 3190, size=8))
shapes.extend(pcb_add)
pcb["BBox"] = {"x": X0, "y": Y0, "width": X1 - X0, "height": Y1 - Y0}

# ---------------------------------------------------------------------------
# Schematic: add sheet 2
# ---------------------------------------------------------------------------
sheet2 = copy.deepcopy(sch["schematics"][0])
sheet2["title"] = "IO_expansion"
sheet2["dataStr"]["head"]["uuid"] = hexid(32)
sheet2["dataStr"]["shape"] = s2
sch["schematics"].append(sheet2)

# ---------------------------------------------------------------------------
# Cross-check: every new schematic pin net == its PCB pad net
# ---------------------------------------------------------------------------
by_ref = {p.ref: p for p in parts}
errors = 0
for ref, num, net in SCH_NETS:
    pad = by_ref[ref].pads.get(num)
    if (pad or "") != net:
        print("MISMATCH", ref, num, "sch", net, "pcb", pad)
        errors += 1
for p in parts:
    if p.prefix == "U" or p.prefix == "J" or p.prefix == "RN" or p.prefix == "C":
        sch_pins = {n for r, n, _ in SCH_NETS if r == p.ref}
        if sch_pins != set(p.pads):
            print("PIN SET MISMATCH", p.ref, sorted(sch_pins ^ set(p.pads)))
            errors += 1
print("parts added:", len(parts), " cross-check errors:", errors)

os.makedirs(OUT_DIR, exist_ok=True)
json.dump(sch, open(os.path.join(OUT_DIR, "SCH_rp2040-basic_expansion.json"), "w"), indent=2, ensure_ascii=False)
json.dump(pcb, open(os.path.join(OUT_DIR, "PCB_rp2040-basic_expansion.json"), "w"), indent=2, ensure_ascii=False)
sys.exit(1 if errors else 0)
