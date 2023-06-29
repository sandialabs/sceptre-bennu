from xml.dom               import minidom
from xml.etree             import ElementTree
from xml.etree.ElementTree import Element, SubElement

def to_xml(message):
    """Convert a Protobuf PowerSystem message to XML"""

    buses     = dict()
    gen_ids   = dict()
    load_ids  = dict()
    shunt_ids = dict()

    root    = Element('Scenario')
    ul      = Element('UmbraLoad')
    ul.text = 'powerSystem'
    root.append(ul)

    ps     = Element('PowerSolver')
    solver = Element('solver')

    ip      = Element('ip')
    ip.text = 'localhost'
    solver.append(ip)

    port      = Element('port')
    port.text = '9999'
    solver.append(port)

    ps.append(solver)
    root.append(ps)

    system = SubElement(root, 'PowerSystem', { 'name':message.name })

    timer      = Element('queryTimer')
    timer.text = str(5)
    system.append(timer)

    create      = Element('create')
    create.text = 'true'
    system.append(create)

    for b in message.buses:
        bus = SubElement(system, 'Bus', { 'name':b.name, 'number':str(b.number),
          'active':str(b.active).lower(), 'slack':str(b.slack).lower(), 'base_kv':str(b.base_kv) })

        for g in b.generators:
            SubElement(bus, 'Generator', { 'name':g.name,
              'active':str(g.active).lower(), 'voltage':str(g.voltage),
              'mw':str(g.mw), 'mvar':str(g.mvar), 'mw_max':str(g.mw_max),
              'mw_min':str(g.mw_min), 'mvar_max':str(g.mvar_max),
              'mvar_min':str(g.mvar_min), 'agc':str(g.agc).lower(),
              'avr':str(g.avr).lower(), 'bid':str(g.bid) })

        for l in b.loads:
            SubElement(bus, 'Load', { 'name':l.name,
              'active':str(l.active).lower(), 'mw':str(l.mw),
              'mvar':str(l.mvar), 'bid':str(l.bid) })

        for s in b.shunts:
            SubElement(bus, 'Shunt', { 'name':s.name,
              'active':str(s.active).lower(), 'mvar':str(s.nominal_mvar) })

    for b in message.branches:
        SubElement(system, 'Branch', { 'name':b.name, 'source':str(b.source),
          'target':str(b.target), 'circuit':str(b.circuit),
          'active':str(b.active).lower(), 'resistance':str(b.resistance),
          'reactance':str(b.reactance), 'charging':str(b.charging),
          'turns_ratio':str(b.turns_ratio), 'phase_angle':str(b.phase_angle),
          'mva_max':str(b.mva_max) })

    return __pretty_print(root)

###########
# private #
###########

def __pretty_print(element):
    """Return a pretty-printed XML string for the given element"""

    rough    = ElementTree.tostring(element, 'UTF-8')
    reparsed = minidom.parseString(rough)

    return reparsed.toprettyxml(indent = "  ")
