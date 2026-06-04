#!/usr/bin/env python3
import os
import re
import sys
import json
import subprocess
import tempfile

def run_test(chirp_bin, file_path, report_path):
    # Run the interpreter
    # Truncate the report file to prevent reading stale data if the interpreter crashes early
    open(report_path, 'w').close()
    
    expected_stdout = None
    expected_interpreter_exit = None
    expected_script_exit = None
    expect_test_failure = False

    try:
        result = subprocess.run(
            [chirp_bin, "--run-report", report_path, file_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=5
        )
        with open(report_path, "r", encoding="utf-8") as f:
            try:
                report = {"diagnostics": []}
                for line in f:
                    line = line.strip()
                    if not line: continue
                    event = json.loads(line)
                    if event.get("event") == "diagnostic":
                        report["diagnostics"].append(event)
                    elif event.get("event") == "outcome":
                        report["outcome"] = event.get("outcome")
                        report["script_exit"] = event.get("script_exit")
                    elif event.get("event") == "expectations":
                        expected_stdout = event.get("expected_stdout")
                        expected_interpreter_exit = event.get("expected_interpreter_exit")
                        expected_script_exit = event.get("expected_script_exit")
                        expect_test_failure = event.get("expect_test_failure", False)
            except json.JSONDecodeError as e:
                return False, f"Could not read run report: {e}", expect_test_failure
    except subprocess.TimeoutExpired:
        return False, f"Execution timed out after 5 seconds", expect_test_failure
    
    if expected_interpreter_exit is None:
        expected_interpreter_exit = expected_script_exit if expected_script_exit is not None else 0
        
    actual_exit = result.returncode
    actual_stdout = result.stdout
    actual_stderr = result.stderr

    failures = []
    if actual_exit != expected_interpreter_exit:
        failures.append(f"Interpreter exit code mismatch: expected {expected_interpreter_exit}, got {actual_exit}")
        if actual_stderr:
            failures.append(f"Stderr output:\n{actual_stderr.strip()}")

    actual_script_exit = report.get("script_exit")
    if expected_script_exit is not None:
        if report.get("outcome") != "script_exit":
            failures.append(f"Run outcome mismatch: expected script_exit, got {report.get('outcome')!r}")
        if actual_script_exit != expected_script_exit:
            failures.append(f"Script exit code mismatch: expected {expected_script_exit}, got {actual_script_exit}")
    elif report.get("outcome") == "script_exit":
        failures.append(f"Unexpected script exit with code {actual_script_exit}")
            
    if expected_stdout is not None and actual_stdout != expected_stdout:
        failures.append(f"Stdout mismatch:\n  Expected: {repr(expected_stdout)}\n  Actual:   {repr(actual_stdout)}")
        
    if failures:
        return False, "\n".join(failures), expect_test_failure
    return True, "", expect_test_failure

def find_test_files(tests_dir):
    test_files = []
    for dirpath, dirnames, filenames in os.walk(tests_dir):
        dirnames.sort()
        for filename in sorted(filenames):
            if filename.endswith(".chirp"):
                test_files.append(os.path.join(dirpath, filename))
    return sorted(test_files, key=lambda path: os.path.relpath(path, tests_dir))

def print_usage():
    print("Usage: test_conformance.py <path_to_chirp_executable> [--filter TEXT]")

def parse_args(argv):
    if len(argv) <= 1:
        print_usage()
        sys.exit(1)

    chirp_bin = argv[1]
    test_filter = None

    i = 2
    while i < len(argv):
        arg = argv[i]
        if arg == "--filter":
            if i + 1 >= len(argv):
                print("Error: --filter requires a value.")
                print_usage()
                sys.exit(1)
            test_filter = argv[i + 1]
            i += 2
        else:
            print(f"Unknown argument: {arg}")
            print_usage()
            sys.exit(1)

    return chirp_bin, test_filter

def main():
    chirp_bin, test_filter = parse_args(sys.argv)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(script_dir)
    
    if not os.path.exists(chirp_bin):
        print(f"Error: Chirp executable not found at '{chirp_bin}'. Did you build the project?")
        sys.exit(1)
        
    tests_dir = os.path.join(root_dir, "tests")
    test_files = find_test_files(tests_dir)
    if test_filter is not None:
        test_files = [
            path for path in test_files
            if test_filter in os.path.relpath(path, tests_dir)
        ]
    
    if not test_files:
        if test_filter is None:
            print("No .chirp tests found.")
        else:
            print(f"No .chirp tests matched filter: {test_filter}")
        sys.exit(1)
    
    test_word = "test" if len(test_files) == 1 else "tests"
    if test_filter is None:
        print(f"Running {len(test_files)} {test_word}...")
    else:
        print(f"Running {len(test_files)} {test_word} matching filter: {test_filter}")
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
    
    report_fd, report_path = tempfile.mkstemp(suffix=".json", prefix="chirp_report_")
    os.close(report_fd)
    
    try:
        for test_file in test_files:
            name = os.path.relpath(test_file, tests_dir)
            passed, detail, expect_test_failure = run_test(chirp_bin, test_file, report_path)
            
            if passed:
                if expect_test_failure:
                    print(f"{COLOR_RED}[XPASS] {name}{COLOR_RESET} (Unexpectedly passed; please remove `expect_test_failure;)")
                    xpass_count += 1
                else:
                    # N.B. extra space is for alignment with [XFAIL] and [XPASS]
                    print(f"{COLOR_GREEN}[PASS]  {name}{COLOR_RESET}")
                    passed_count += 1
            else:
                if expect_test_failure:
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
    finally:
        if os.path.exists(report_path):
            os.remove(report_path)

if __name__ == "__main__":
    main()
