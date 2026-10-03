import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def method(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def main(source_ref):
    def source(path):
        if source_ref:
            return subprocess.check_output(['git', 'show', f'{source_ref}:{path}'], cwd=ROOT).decode()
        return (ROOT / path).read_text()
    compiler = shutil.which(os.environ.get('CXX', 'cl.exe' if os.name == 'nt' else 'c++'))
    assert compiler, 'Enable a C++20 compiler'
    controller = source('src/CoAContentScaling.cpp')
    bodies = '\n'.join(method(controller, signature) for signature in (
        'void CoAContentScaling::FinalizeAndInitialize()',
        'void CoAContentScaling::UnregisterLocalLevelScalingHooks()',
        'void CoAContentScaling::OnResolveLfgQueuePolicy(',
        'uint8 CoAContentScaling::GetEffectiveCreatureLevel(',
        'int32 CoAContentScaling::GetEffectiveQuestLevel(',
        'uint32 CoAContentScaling::GetEffectiveQuestMinLevel(',
    ))
    bodies += '\n' + method(source('src/ItemBudgetScaler.cpp'), 'ItemScalingContext ItemScalingContext::Resolve(')
    keys = source('include/CoAContentScalingConfig.h')
    keys = keys[keys.index('namespace CoAContentScalingConfigKeys'):keys.index('namespace CoAContentScalingConfig\n')]
    harness = (ROOT / 'tools/content_census/scaling_controls_regression.cpp').read_text()
    harness = harness.replace('// ACTUAL_KEYS', keys).replace('// ACTUAL_METHODS', bodies)
    with tempfile.TemporaryDirectory(prefix='coa-scaling-controls-') as folder:
        out = Path(folder)
        for name in ('ContentPackRegistry', 'ProgressionLayout', 'ContentEra', 'ContentTier', 'InstanceProfile',
                     'GeneratedContentCensus', 'ItemBudgetScaler'):
            (out / f'{name}.h').write_text(source(f'include/{name}.h'), encoding='utf-8')
        for name in ('ContentPackRegistry', 'ProgressionLayout'):
            (out / f'{name}.cpp').write_text(source(f'src/{name}.cpp'), encoding='utf-8')
        (out / 'Define.h').write_text('#pragma once\n#include <cstdint>\nusing uint8=std::uint8_t; using uint16=std::uint16_t; using uint32=std::uint32_t; using int16=std::int16_t; using int32=std::int32_t;\n')
        (out / 'Log.h').write_text('#pragma once\n#define LOG_INFO(...)\n#define LOG_ERROR(...)\n#define LOG_WARN(...)\n')
        (out / 'regression.cpp').write_text(harness, encoding='utf-8')
        exe = out / ('regression.exe' if os.name == 'nt' else 'regression')
        files = ['regression.cpp', 'ContentPackRegistry.cpp', 'ProgressionLayout.cpp']
        flags = (['/nologo', '/std:c++20', '/EHsc', '/utf-8', *files, '/Fe' + str(exe)]
                 if Path(compiler).stem.lower() == 'cl' else ['-std=c++20', *files, '-o', str(exe)])
        subprocess.run([compiler, *flags], cwd=out, check=True)
        subprocess.run([str(exe)], cwd=out, check=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-ref')
    main(parser.parse_args().source_ref)
