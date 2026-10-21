#!/usr/bin/env python3
"""Split a source file into several ones.

The tool takes a source file, a list of new files and, for each of them,
the names of the functions that belong in it.  Every function is copied
over with its doc comment, so the result is the same code in a new place.
"""

import io
import os
import re


def load(path):
    return io.open(path, encoding="utf-8").read().split("\n")


class Source(object):
    def __init__(self, path):
        self.lines = load(path)
        self.used = set()

    def find(self, name):
        """The lines of a function (with its doc comment) and its body."""
        pat = re.compile(r"^[A-Za-z_].*\b" + re.escape(name) + r"\s*\(")
        hit = None
        for i, line in enumerate(self.lines):
            if i in self.used:
                continue
            if pat.match(line) and not line.rstrip().endswith(";"):
                hit = i
                break
        if hit is None:
            raise SystemExit("cannot find the definition of %s" % name)

        depth = 0
        opened = False
        end = hit
        for j in range(hit, len(self.lines)):
            depth += self.lines[j].count("{") - self.lines[j].count("}")
            if "{" in self.lines[j]:
                opened = True
            if opened and depth == 0:
                end = j
                break

        first = hit
        if hit > 0 and self.lines[hit - 1].strip() == "*/":
            k = hit - 1
            while k >= 0 and self.lines[k].strip() != "/*":
                k -= 1
            if k >= 0:
                first = k

        for i in range(first, end + 1):
            self.used.add(i)
        return self.lines[first:end + 1]

    def raw(self, first_line, last_line):
        """A block of lines that is not a function."""
        start = None
        for i, line in enumerate(self.lines):
            if line.startswith(first_line):
                start = i
                break
        if start is None:
            raise SystemExit("cannot find %r" % first_line)
        end = start
        for j in range(start, len(self.lines)):
            if self.lines[j].rstrip() == last_line:
                end = j
                break
        for i in range(start, end + 1):
            self.used.add(i)
        return self.lines[start:end + 1]


def write(path, chunk):
    body = "\n".join(chunk).rstrip("\n") + "\n"

    # no double blank lines, and a blank line before every banner
    out = []
    for line in body.split("\n"):
        if line.strip() == "" and out and out[-1].strip() == "":
            continue
        if (line.startswith("/* ---") and out and out[-1].strip() != ""
                and not re.match(r"^/\*  .*\*/$", out[-1])):
            out.append("")
        out.append(line)
    body = "\n".join(out)

    directory = os.path.dirname(path)
    if directory and not os.path.isdir(directory):
        os.makedirs(directory)
    io.open(path, "w", encoding="utf-8").write(body)
    print("%-30s %4d lines" % (path, body.count("\n")))


def literal_mask(t):
    mask = [True] * len(t)
    i = 0
    n = len(t)
    while i < n:
        c = t[i]
        if c == '"' or c == "'":
            quote = c
            i += 1
            while i < n:
                if t[i] == "\\":
                    i += 2
                    continue
                if t[i] == quote:
                    i += 1
                    break
                mask[i] = False
                i += 1
            continue
        i += 1
    return mask


def rename(t, old, new):
    """Rename a whole word outside of string literals."""
    mask = literal_mask(t)
    pat = re.compile(r"\b" + re.escape(old) + r"\b")
    out = []
    last = 0
    for m in pat.finditer(t):
        if not mask[m.start()]:
            continue
        out.append(t[last:m.start()])
        out.append(new)
        last = m.end()
    out.append(t[last:])
    return "".join(out)


def split(spec):
    src = Source(spec["src"])

    for group in spec["groups"]:
        chunk = [group["header"].rstrip("\n")]

        for raw in group.get("raw", []):
            chunk.append("")
            chunk.extend(src.raw(raw[0], raw[1]))

        for name in group.get("functions", []):
            chunk.append("")
            chunk.extend(src.find(name))

        body = "\n".join(chunk)
        for old, new in spec.get("renames", []):
            body = rename(body, old, new)
        for pattern, repl in spec.get("fixups", []):
            body = re.sub(pattern, repl, body, flags=re.MULTILINE)
        write(group["out"], body.split("\n"))
