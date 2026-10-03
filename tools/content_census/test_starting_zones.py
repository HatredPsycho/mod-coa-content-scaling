import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from generate_census import is_starting_area, placement_era

ROOT = Path(__file__).resolve().parents[2]

class StartingAreasTest(unittest.TestCase):
    def test_roots_children_and_cycles(self):
        areas = {3431: {"parent_zone": 10141}, 3526: {"parent_zone": 10142},
                 90000: {"parent_zone": 3431}, 90001: {"parent_zone": 90002},
                 90002: {"parent_zone": 90001}}
        for area in (3430, 3433, 3487, 3524, 3525, 3557, 3431, 3526, 10141, 10142, 90000):
            self.assertTrue(is_starting_area(area, areas))
        for area in (0, 3483, 3520, 90001):
            self.assertFalse(is_starting_area(area, areas))

    def test_placement_is_not_expansion_release(self):
        areas = {3431: {"parent_zone": 3430}, 3526: {"parent_zone": 3524}}
        self.assertEqual(placement_era(530, {3431}, areas, 1, 60, 1), "Classic")
        self.assertEqual(placement_era(530, {3526}, areas, 0, 1, 1), "Classic")
        self.assertEqual(placement_era(530, {0}, areas, 0, 1, 1), "Classic")
        self.assertEqual(placement_era(530, {3483}, areas, 0, 1, 1), "TBC")
        self.assertEqual(placement_era(530, {3431, 3483}, areas, 0, 1, 1), "TBC")
        self.assertEqual(placement_era(530, {0}, areas, 1, 60, 1), "TBC")
        self.assertEqual(placement_era(571, {0}, areas, 0, 1, 2), "WotLK")

def native_regression(source_ref):
    compiler = shutil.which(os.environ.get('CXX', 'cl.exe' if os.name == 'nt' else 'c++'))
    assert compiler, 'Enable a C++20 compiler'
    def source(path):
        if source_ref:
            return subprocess.check_output(['git', 'show', f'{source_ref}:{path}'], cwd=ROOT).decode()
        return (ROOT / path).read_text()
    with tempfile.TemporaryDirectory(prefix='coa-starting-zones-') as folder:
        out = Path(folder)
        for name in ('ContentPackRegistry', 'ProgressionLayout', 'ContentEra', 'ContentTier', 'InstanceProfile', 'GeneratedContentCensus'):
            (out / f'{name}.h').write_text(source(f'include/{name}.h'), encoding='utf-8')
        for name in ('ContentPackRegistry', 'ProgressionLayout'):
            (out / f'{name}.cpp').write_text(source(f'src/{name}.cpp'), encoding='utf-8')
        (out / 'Define.h').write_text('''#pragma once
#include <cstdint>
using uint8=std::uint8_t; using uint16=std::uint16_t; using uint32=std::uint32_t;
using int16=std::int16_t; using int32=std::int32_t;
''')
        (out / 'Log.h').write_text('#pragma once\n#define LOG_INFO(...)\n#define LOG_ERROR(...)\n')
        (out / 'regression.cpp').write_text((ROOT / 'tools/content_census/starting_zones_regression.cpp').read_text())
        exe = out / ('regression.exe' if os.name == 'nt' else 'regression')
        files = ['regression.cpp', 'ContentPackRegistry.cpp', 'ProgressionLayout.cpp']
        flags = (['/nologo', '/std:c++20', '/EHsc', '/utf-8', *files, '/Fe' + str(exe)]
                 if Path(compiler).stem.lower() == 'cl' else ['-std=c++20', *files, '-o', str(exe)])
        subprocess.run([compiler, *flags], cwd=out, check=True)
        subprocess.run([str(exe)], cwd=out, check=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-ref')
    args = parser.parse_args()
    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(StartingAreasTest))
    if not result.wasSuccessful():
        raise SystemExit(1)
    native_regression(args.source_ref)
