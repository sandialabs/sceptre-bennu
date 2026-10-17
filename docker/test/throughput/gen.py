#!/usr/bin/env python3
"""Emit a field-device config with N output-mapped analog tags that all change every cycle."""
import sys
n, cycle_ms, ep = int(sys.argv[1]), sys.argv[2], sys.argv[3]
t = "".join(f"<external-tag><name>p{i}</name><io>O{i}</io><type>analog</type></external-tag>" for i in range(n))
o = "".join(f"<analog><id>O{i}</id><name>p{i}</name></analog>" for i in range(n))
l = "\n".join(f"p{i} = p{i} + 1" for i in range(n))
print(f"""<?xml version="1.0" encoding="utf-8"?>
<SCEPTRE><field-device><name>tp-1</name><cycle-time>{cycle_ms}</cycle-time>
<tags>{t}</tags>
<output><endpoint>{ep}</endpoint>{o}</output>
<logic><![CDATA[
{l}
]]></logic></field-device></SCEPTRE>""")
