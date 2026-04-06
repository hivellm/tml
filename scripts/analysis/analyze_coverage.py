#!/usr/bin/env python3
"""
TML Library Coverage Analyzer
Shows which library modules/functions are NOT covered by tests.
"""

import os
import re
import sys
from pathlib import Path
from collections import defaultdict

def extract_functions_from_file(filepath):
    """Extract function and method names from a TML file."""
    functions = []
    current_impl = None
    
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except:
        return functions
    
    lines = content.split('\n')
    
    for line in lines:
        # Track impl blocks
        impl_match = re.match(r'^\s*impl\s+(\w+)', line)
        if impl_match:
            current_impl = impl_match.group(1)
        
        # End of impl block (simple heuristic)
        if line.strip() == '}' and current_impl:
            current_impl = None
        
        # Match function definitions
        func_match = re.match(r'^\s*(pub\s+)?func\s+(\w+)', line)
        if func_match:
            func_name = func_match.group(2)
            # Skip test functions
            if func_name.startswith('test_'):
                continue
            
            if current_impl:
                functions.append(f"{current_impl}.{func_name}")
            else:
                functions.append(func_name)
    
    return functions

def get_module_from_path(filepath, base_dir):
    """Get module name from file path."""
    rel = os.path.relpath(filepath, base_dir)
    rel = rel.replace(os.sep, '/').replace('/src/', '/').replace('.tml', '')
    parts = rel.split('/')
    if parts[-1] == 'mod':
        parts = parts[:-1]
    return '/'.join(parts)

def scan_library(lib_dirs):
    """Scan library directories for all functions."""
    modules = defaultdict(list)
    
    for lib_dir in lib_dirs:
        base = Path(lib_dir)
        if not base.exists():
            continue
        
        for tml_file in base.rglob('*.tml'):
            str_file = str(tml_file)
            # Skip test files
            if '.test.tml' in str_file or '/tests/' in str_file.replace(os.sep, '/'):
                continue
            
            funcs = extract_functions_from_file(tml_file)
            if funcs:
                module = get_module_from_path(str_file, str(base))
                modules[module].extend(funcs)
    
    return modules

def parse_coverage_html(html_path):
    """Extract covered functions from coverage.html."""
    covered = set()
    
    try:
        with open(html_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except:
        return covered
    
    # Extract function names from table rows
    pattern = r'<td>([^<]+)</td>\s*<td class="calls">(\d+)</td>'
    for match in re.finditer(pattern, content):
        func_name = match.group(1)
        calls = int(match.group(2))
        if calls > 0:
            covered.add(func_name)
    
    return covered

def main():
    base = Path(__file__).parent.parent
    
    # Scan library
    lib_dirs = [
        base / 'lib' / 'core',
        base / 'lib' / 'std',
    ]
    
    modules = scan_library(lib_dirs)
    
    # Get covered functions
    coverage_file = base / 'coverage.html'
    covered = parse_coverage_html(coverage_file)
    
    # Add builtin methods that we track
    builtin_methods = [
        'Slice.len', 'Slice.is_empty',
        'MutSlice.len', 'MutSlice.is_empty',
        'Array.len', 'Array.is_empty', 'Array.get', 'Array.first', 'Array.last', 'Array.map', 'Array.eq', 'Array.ne', 'Array.cmp',
        'Maybe.is_just', 'Maybe.is_nothing', 'Maybe.unwrap', 'Maybe.unwrap_or', 'Maybe.map',
    ]
    
    # Add builtins to report
    modules['core/builtins'] = builtin_methods
    
    # Print report
    print()
    print("="*80)
    print("               TML LIBRARY COVERAGE REPORT")
    print("="*80)
    print()
    
    total_funcs = 0
    total_covered = 0
    uncovered_by_module = defaultdict(list)
    
    for module in sorted(modules.keys()):
        funcs = modules[module]
        module_covered = sum(1 for f in funcs if f in covered)
        module_total = len(funcs)
        total_funcs += module_total
        total_covered += module_covered
        
        # Track uncovered
        for f in funcs:
            if f not in covered:
                uncovered_by_module[module].append(f)
    
    # Summary
    overall_pct = (total_covered / total_funcs * 100) if total_funcs > 0 else 0
    print(f"Overall Coverage: {total_covered}/{total_funcs} functions ({overall_pct:.1f}%)")
    print()
    
    # Per-module summary
    print("-"*80)
    print(f"{'Module':<40} {'Coverage':>15} {'Percent':>10}")
    print("-"*80)
    
    for module in sorted(modules.keys()):
        funcs = modules[module]
        module_covered = sum(1 for f in funcs if f in covered)
        module_total = len(funcs)
        pct = (module_covered / module_total * 100) if module_total > 0 else 0
        
        if pct == 100:
            status = "+"
        elif pct == 0:
            status = "X"
        else:
            status = "~"
        print(f"{status} {module:<38} {module_covered:>5}/{module_total:<5} {pct:>9.1f}%")
    
    # Show uncovered functions
    print()
    print("="*80)
    print("               UNCOVERED FUNCTIONS (0% coverage modules)")
    print("="*80)
    print()
    
    # First show modules with 0% coverage
    for module in sorted(uncovered_by_module.keys()):
        funcs = modules[module]
        module_covered = sum(1 for f in funcs if f in covered)
        if module_covered == 0 and len(funcs) > 0:
            print(f"{module}: ({len(funcs)} functions)")
            for f in sorted(uncovered_by_module[module])[:10]:
                print(f"    - {f}")
            if len(uncovered_by_module[module]) > 10:
                print(f"    ... and {len(uncovered_by_module[module]) - 10} more")
            print()
    
    print("="*80)

if __name__ == '__main__':
    main()
