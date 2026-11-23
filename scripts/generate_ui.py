"""Generate matched LV2 factory presets and native UI control metadata."""

import argparse
import json
from pathlib import Path
import re

BASE = "https://weikiboy-tech.github.io/voiceforge-lv2/"
TRANSIENT = {"meter_reset", "analyze", "analysis_reset"}


def read_ports(path):
    number = lambda body, field: re.search(rf"lv2:{field}\s+(-?[\d.]+)", body).group(1)
    entries = []
    for direction, body in re.findall(r"\[\s*a lv2:(Input|Output)Port,\s*lv2:ControlPort\s*;(.*?)\](?=[,.])",
                                      path.read_text(encoding="utf8"), re.S):
        symbol = re.search(r'lv2:symbol "([^"]+)"', body).group(1)
        name = re.search(r'lv2:name "([^"]+)"', body).group(1)
        index = int(number(body, "index"))
        unit = re.search(r"units:unit units:(\w+)", body)
        output = direction == "Output"
        has_limits = "lv2:minimum" in body
        lo, hi = (number(body, "minimum"), number(body, "maximum")) if has_limits else ("0", "100000")
        default = number(body, "default") if not output else ("0" if symbol == "sample_rate" else lo)
        entries.append((index, symbol, name, lo, hi, default,
                        unit.group(1) if unit else "", output, "lv2:toggled" in body or symbol == "mode"))
    return entries


def float_literal(value):
    return f"{float(value):.8f}f"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("header", type=Path)
    parser.add_argument("bundle", type=Path)
    args = parser.parse_args()
    ports = read_ports(args.source / "lv2/voiceforge-mono.ttl")
    stereo = read_ports(args.source / "lv2/voiceforge-stereo.ttl")
    if [(i - 2, *fields) for i, *fields in ports] != [(i - 4, *fields) for i, *fields in stereo]:
        raise ValueError("Mono and stereo control metadata disagree.")
    presets = json.loads((args.source / "presets/factory.json").read_text(encoding="utf8"))
    input_ports = {p[1]: p for p in ports if not p[-2] and p[1] not in TRANSIENT}
    for values in presets.values():
        for symbol, value in values.items():
            entry = input_ports[symbol]
            if not float(entry[3]) <= value <= float(entry[4]):
                raise ValueError(f"Factory preset contains an out-of-range value for {symbol}")
    h = ["#pragma once", "namespace voiceforge::ui {", "struct Param {",
         "  const char* symbol; const char* label; float min, max, initial;",
         "  const char* unit; bool output, toggle;", "};", "inline constexpr Param params[] = {"]
    quoted = lambda value: json.dumps(value, ensure_ascii=False)
    for _, symbol, name, lo, hi, d, unit, output, toggle in ports:
        fields = [quoted(symbol), quoted(name), *[float_literal(x) for x in (lo, hi, d)], quoted(unit),
                  str(output).lower(), str(toggle).lower()]
        h.append("  {" + ", ".join(fields) + "},")
    h.extend(["};", "inline constexpr const char* factoryNames[] = {" +
              ", ".join(quoted(x) for x in presets) + "};",
              "inline constexpr float factory[][sizeof(params) / sizeof(params[0])] = {"])
    for values in presets.values():
        h.append("  {" + ", ".join(float_literal(values.get(p[1], p[5])) for p in ports) + "},")
    h.extend(["};", "} // namespace voiceforge::ui"])
    args.header.write_text("\n".join(h) + "\n", encoding="utf8")

    prefix = """@prefix lv2: <http://lv2plug.in/ns/lv2core#> .
@prefix pset: <http://lv2plug.in/ns/ext/presets#> .
@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .
@prefix ui: <http://lv2plug.in/ns/extensions/ui#> .
@prefix urid: <http://lv2plug.in/ns/ext/urid#> .
"""
    manifest = (args.source / "lv2/manifest.ttl").read_text(encoding="utf8")
    manifest += "\n" + "\n".join(prefix.splitlines()[1:]) + "\n"
    manifest += f"\n<{BASE}ui> a ui:X11UI ; lv2:binary <voiceforge_ui.so> ; rdfs:seeAlso <voiceforge-ui.ttl> .\n"
    ui = prefix + f"""\n<{BASE}ui>
    a ui:X11UI ;
    lv2:binary <voiceforge_ui.so> ;
    lv2:requiredFeature ui:parent, ui:idleInterface ;
    lv2:optionalFeature ui:resize, ui:touch, urid:map ;
    lv2:extensionData ui:idleInterface, ui:resize ;
    ui:portNotification
"""
    notifications = []
    for variant in ("mono", "stereo"):
        notifications.extend(f'        [ ui:plugin <{BASE}{variant}> ; lv2:symbol "{p[1]}" ;'
                             ' ui:protocol ui:floatProtocol ]' for p in ports if p[-2])
        notifications.append(f'        [ ui:plugin <{BASE}{variant}> ; lv2:symbol "compare_out" ;'
                             ' ui:protocol <http://lv2plug.in/ns/ext/atom#eventTransfer> ]')
    ui += ",\n".join(notifications) + " .\n"
    presets_ttl = prefix
    for variant in ("mono", "stereo"):
        plugin = f"<{BASE}{variant}>"
        source = (args.source / f"lv2/voiceforge-{variant}.ttl").read_text(encoding="utf8")
        (args.bundle / f"voiceforge-{variant}.ttl").write_text(
            source + f"\n{plugin} <http://lv2plug.in/ns/extensions/ui#ui> <{BASE}ui> .\n", encoding="utf8")
        for name, values in presets.items():
            uri = f"<{BASE}preset/{variant}/{name.lower().replace(' ', '-')}>"
            manifest += f"\n{uri} a pset:Preset ; lv2:appliesTo {plugin} ; rdfs:seeAlso <presets.ttl> .\n"
            controls = ",\n".join('        [ lv2:symbol "' + p[1] + '" ; pset:value ' +
                                    str(float(values.get(p[1], p[5]))) + " ]" for p in input_ports.values())
            presets_ttl += f"\n{uri}\n    a pset:Preset ; lv2:appliesTo {plugin} ;\n" \
                           f"    rdfs:label {quoted(name)} ;\n    lv2:port\n{controls} .\n"
    (args.bundle / "manifest.ttl").write_text(manifest, encoding="utf8")
    (args.bundle / "voiceforge-ui.ttl").write_text(ui, encoding="utf8")
    (args.bundle / "presets.ttl").write_text(presets_ttl, encoding="utf8")


if __name__ == "__main__":
    main()
