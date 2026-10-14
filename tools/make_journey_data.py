#!/usr/bin/env python3
"""Builds include/JourneyData.h from data/journey/: the zones with the original entry levels of the client, and the
instances with their entry levels and creature levels, as they came. The module lays them over the journey at every
start, by its regulators.

The tables and their layout come from mod-world-journey by aspollon (tools/make_levels.py, faed6d6)."""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
DATA = os.path.join(ROOT, 'data', 'journey')
OUT = os.path.join(ROOT, 'include', 'JourneyData.h')
PART = {'classic': 0, 'tbc': 1, 'wotlk': 2}
RAIDS = {249, 309, 409, 469, 509, 531, 532, 533, 534, 544, 548, 550, 564, 565, 568, 580, 603, 615, 616, 624, 631, 649, 724}


def lines(name):
    with open(os.path.join(DATA, name), encoding='utf-8') as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            yield line.rstrip('\n')


def build():
    zones = []
    for line in lines('source_zones.tsv'):
        key, area, mapid, low, _high, era = line.split()
        zones.append((int(area), int(mapid), PART[era], int(low), key))

    content = {}
    with open(os.path.join(DATA, 'source_instance_levels.txt'), encoding='utf-8') as f:
        for part in f.read().strip().split(';'):
            mapid, _low, average, _high, _count = map(int, part.split())
            content[mapid] = average

    instances = []
    for line in lines('source_instance_access.tsv'):
        mapid, difficulty, low, _high, name = line.split('\t')
        mapid, difficulty, low = int(mapid), int(difficulty), int(low)
        kind = 2 if mapid in RAIDS else 1 if difficulty > 0 else 0
        instances.append((mapid, difficulty, kind, low, max(content.get(mapid, low), low), name.replace('"', "'")))

    out = [
        '/*',
        ' * CoA Universal Content Scaling',
        ' * JourneyData: the original zone and instance levels the journey layout is laid over.',
        ' * Built by tools/make_journey_data.py from data/journey/ - do not edit by hand.',
        ' */',
        '',
        '#ifndef COA_JOURNEY_DATA_H',
        '#define COA_JOURNEY_DATA_H',
        '',
        '#include <cstdint>',
        '',
        'namespace journey',
        '{',
        '    struct ZoneData { uint32_t zone; uint32_t map; uint8_t part; uint8_t originalEntry; char const* key; };',
        '    struct InstanceData',
        '    {',
        '        uint32_t map;',
        '        uint8_t difficulty;',
        '        uint8_t kind;',
        '        uint8_t originalEntry;',
        '        uint8_t contentLevel;',
        '        char const* name;',
        '    };',
        '',
        '    inline constexpr ZoneData Zones[] =',
        '    {',
    ]
    out += [f'        {{ {z[0]}, {z[1]}, {z[2]}, {z[3]}, "{z[4]}" }},' for z in zones]
    out += ['    };', '', '    inline constexpr InstanceData Instances[] =', '    {']
    out += [f'        {{ {i[0]}, {i[1]}, {i[2]}, {i[3]}, {i[4]}, "{i[5]}" }},' for i in instances]
    out += ['    };', '}', '', '#endif // COA_JOURNEY_DATA_H', '']

    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out))
    return len(zones), len(instances)


if __name__ == '__main__':
    zone_count, instance_count = build()
    print(zone_count, 'zones,', instance_count, 'instance entries')
