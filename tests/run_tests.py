#!/usr/bin/env python3
import os
import re
import sys
import subprocess

def parse_expectations(file_path):
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()
    
    # Extract expected stdout. Can be multiple calls, which we concatenate.
    stdout_matches = re.findall(r'`expect_stdout\("((?:[^"\\]|\\.)*)"\);', content)
    expected_stdout = ""
    for match in stdout_matches:
        decoded = match.replace('\\n', '\n').replace('\\t', '\t').replace('\\"', '"').replace('\\\\', '\\')
        expected_stdout += decoded
        
    # Extract expected exit code (defaults to 0 if not specified)
    exit_match = re.search(r'`expect_exit\((\d+)\);', content)
    expected_exit = int(exit_match.group(1)) if exit_match else 0
    
    # Check for `expect_failure; tag
    expect_failure = bool(re.search(r'`expect_failure\s*;', content))
    
    return expected_stdout, expected_exit, expect_failure

def run_test(chirp_bin, file_path):
    expected_stdout, expected_exit, expect_failure = parse_expectations(file_path)
    
    # Run the interpreter
    try:
        result = subprocess.run(
            [chirp_bin, file_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=5
        )
    except subprocess.TimeoutExpired:
        return False, f"Execution timed out after 5 seconds", expect_failure
    
    actual_exit = result.returncode
    actual_stdout = result.stdout
    actual_stderr = result.stderr

    failures = []
    if actual_exit != expected_exit:
        failures.append(f"Exit code mismatch: expected {expected_exit}, got {actual_exit}")
        if actual_stderr:
            failures.append(f"Stderr output:\n{actual_stderr.strip()}")
            
    if expected_stdout != "" and actual_stdout != expected_stdout:
        failures.append(f"Stdout mismatch:\n  Expected: {repr(expected_stdout)}\n  Actual:   {repr(actual_stdout)}")
        
    if failures:
        return False, "\n".join(failures), expect_failure
    return True, "", expect_failure

def main():
    # Find chirp executable
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(script_dir)
    chirp_bin = os.path.join(root_dir, "build", "app", "chirp")
    
    if not os.path.exists(chirp_bin):
        print(f"Error: Chirp executable not found at '{chirp_bin}'. Did you build the project?")
        sys.exit(1)
        
    # Find all test files
    test_files = sorted([
        os.path.join(script_dir, f)
        for f in os.listdir(script_dir)
        if f.endswith(".chirp")
    ])
    
    if not test_files:
        print("No .chirp tests found.")
        sys.exit(1)
    
    test_word = "test" if len(test_files) == 1 else "tests"
    print(f"Running {len(test_files)} {test_word}...")
    print("=" * 60)
    
    passed_count = 0
    failed_count = 0
    xfail_count = 0
    xpass_count = 0
    
    # ANSI Escape Codes for Colors
    COLOR_GREEN = "\033[92m"
    COLOR_RED = "\033[91m"
    COLOR_YELLOW = "\033[93m"
    COLOR_RESET = "\033[0m"
    
    for test_file in test_files:
        name = os.path.basename(test_file)
        passed, detail, expect_failure = run_test(chirp_bin, test_file)
        
        if passed:
            if expect_failure:
                print(f"{COLOR_RED}[XPASS] {name}{COLOR_RESET} (Unexpectedly passed; please remove `expect_failure;)")
                xpass_count += 1
            else:
                # N.B. extra space is for alignment with [XFAIL] and [XPASS]
                print(f"{COLOR_GREEN}[PASS]  {name}{COLOR_RESET}")
                passed_count += 1
        else:
            if expect_failure:
                print(f"{COLOR_YELLOW}[XFAIL] {name}{COLOR_RESET}")
                xfail_count += 1
            else:
                # N.B. extra space is for alignment with [XFAIL] and [XPASS]
                print(f"{COLOR_RED}[FAIL]  {name}{COLOR_RESET}")
                print("-" * 40)
                print(detail)
                print("-" * 40)
                failed_count += 1
            
    print("=" * 60)
    print(f"Summary: {passed_count} passed, {xfail_count} expected failures (XFAIL)")
    if xpass_count > 0 or failed_count > 0:
        print(f"Errors: {failed_count} unexpected failures, {xpass_count} unexpected passes (XPASS)")
        sys.exit(1)
    else:
        sys.exit(0)

if __name__ == "__main__":
    main()
