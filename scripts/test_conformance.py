#!/usr/bin/env python3
import os
import re
import sys
import json
import subprocess
import tempfile
import concurrent.futures
import threading
import argparse

# Thread-local storage to recycle report files per thread/worker and minimize filesystem metadata churn.
# To ensure maximum portability across Linux, macOS, BSD, and Windows (avoiding sharing violations/locks),
# we do not keep the file open in Python during the interpreter's run. Instead, we create a single persistent
# file path per thread, and open/close it in Python before and after the run. This completely avoids directory
# metadata allocation/deallocation churn while ensuring clean concurrency boundaries.
thread_local = threading.local()
temp_files_registry = []
registry_lock = threading.Lock()
runner_counter = 0
counter_lock = threading.Lock()

# ANSI Escape Codes for Colors
COLOR_GREEN = "\033[92m"
COLOR_RED = "\033[91m"
COLOR_YELLOW = "\033[93m"
COLOR_RESET = "\033[0m"

def print_test_status(status, name, color, extra=""):
    status_padded = status.ljust(7)
    print(f"{color}{status_padded} {name}{COLOR_RESET}{extra}")

def get_thread_info():
    if not hasattr(thread_local, "report_path"):
        global runner_counter
        report_fd, report_path = tempfile.mkstemp(suffix=".json", prefix="chirp_report_")
        os.close(report_fd)
        thread_local.report_path = report_path
        with counter_lock:
            runner_counter += 1
            thread_local.runner_id = runner_counter
        with registry_lock:
            temp_files_registry.append(report_path)
    return thread_local.report_path, thread_local.runner_id

def run_test(chirp_bin, file_path):
    report_path, runner_id = get_thread_info()
    
    # Truncate the report file to prevent reading stale data from a previous test run on this thread
    try:
        with open(report_path, 'w') as f:
            pass
    except OSError as e:
        return False, f"Could not truncate report file {report_path}: {e}", False, runner_id
    
    expected_stdout = None
    expected_exit = None
    expect_test_failure = False

    try:
        result = subprocess.run(
            [chirp_bin, "--test", "--run-report", report_path, file_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=5
        )
        report = {"diagnostics": []}
        if os.path.exists(report_path):
            with open(report_path, "r", encoding="utf-8") as f:
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
                        expected_exit = event.get("expected_exit")
                        expect_test_failure = event.get("expect_test_failure", False)
                    elif event.get("event") == "expectation_checks":
                        report["expectation_checks"] = event.get("count", 0)
    except json.JSONDecodeError as e:
        return False, f"Could not read run report: {e}", expect_test_failure, runner_id
    except subprocess.TimeoutExpired:
        return False, f"Execution timed out after 5 seconds", expect_test_failure, runner_id
    
    is_syntax_test = "/syntax/" in file_path.replace(os.sep, "/")
    if is_syntax_test:
        expected_exit = 1

    if expected_exit is None:
        expected_exit = 0
        
    actual_exit = result.returncode
    actual_stdout = result.stdout
    actual_stderr = result.stderr

    failures = []
    if actual_exit != expected_exit:
        failures.append(f"Interpreter exit code mismatch: expected {expected_exit}, got {actual_exit}")
        if actual_stderr:
            failures.append(f"Stderr output:\n{actual_stderr.strip()}")

    if is_syntax_test:
        actual_outcome = report.get("outcome")
        if actual_outcome != "syntax_failure":
            failures.append(f"Run outcome mismatch: expected syntax_failure, got {actual_outcome!r}")
            
    if expected_stdout is not None and actual_stdout != expected_stdout:
        failures.append(f"Stdout mismatch:\n  Expected: {repr(expected_stdout)}\n  Actual:   {repr(actual_stdout)}")
        
    if failures:
        return False, "\n".join(failures), expect_test_failure, runner_id
    return True, "", expect_test_failure, runner_id

def find_test_files(tests_dir):
    test_files = []
    for dirpath, dirnames, filenames in os.walk(tests_dir):
        dirnames.sort()
        for filename in sorted(filenames):
            if filename.endswith(".chirp"):
                test_files.append(os.path.join(dirpath, filename))
    return sorted(test_files, key=lambda path: os.path.relpath(path, tests_dir))

def parse_args():
    parser = argparse.ArgumentParser(
        description="Run Chirp conformance test suite in sequential or parallel mode."
    )
    parser.add_argument(
        "--filter",
        help="Filter tests by name substring"
    )
    parser.add_argument(
        "-j", "--jobs",
        default="1",
        help="Run tests in parallel. Can be an integer or 'auto' (which uses the system CPU count). Defaults to 1."
    )
    parser.add_argument(
        "--suite",
        help="Path to a custom test suite directory (defaults to the project's tests/ directory)"
    )
    parser.add_argument(
        "chirp_bin",
        help="Path to the Chirp executable under test"
    )
    
    args = parser.parse_args()
    
    # Process jobs parameter
    if args.jobs == "auto":
        cpu_count = os.cpu_count()
        jobs = cpu_count if cpu_count is not None else 2
    elif re.match(r'^\d+$', args.jobs):
        jobs = int(args.jobs)
    else:
        jobs = 1
        
    return args.chirp_bin, args.filter, jobs, args.suite

def main():
    chirp_bin, test_filter, jobs, suite_dir = parse_args()
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(script_dir)
    
    if not os.path.exists(chirp_bin):
        print(f"Error: Chirp executable not found at '{chirp_bin}'. Did you build the project?")
        sys.exit(1)
        
    if suite_dir:
        tests_dir = os.path.abspath(suite_dir)
    else:
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
    
    try:
        if jobs > 1:
            with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as executor:
                future_to_name = {
                    executor.submit(run_test, chirp_bin, test_file): os.path.relpath(test_file, tests_dir)
                    for test_file in test_files
                }
                
                for future in concurrent.futures.as_completed(future_to_name):
                    name = future_to_name[future]
                    runner_id = "?"
                    try:
                        passed, detail, expect_test_failure, runner_id = future.result()
                        if passed:
                            if expect_test_failure:
                                print_test_status("[XPASS]", name, COLOR_RED, " (Unexpectedly passed; please remove `expect_test_failure;)")
                                xpass_count += 1
                            else:
                                print_test_status("[PASS]", name, COLOR_GREEN)
                                passed_count += 1
                        else:
                            if expect_test_failure:
                                print_test_status("[XFAIL]", name, COLOR_YELLOW)
                                xfail_count += 1
                            else:
                                print_test_status("[FAIL]", name, COLOR_RED)
                                print("-" * 40)
                                print(detail)
                                print("-" * 40)
                                failed_count += 1
                    except Exception as exc:
                        print_test_status("[ERROR]", name, COLOR_RED, f" raised an exception: {exc}")
                        failed_count += 1
        else:
            for test_file in test_files:
                name = os.path.relpath(test_file, tests_dir)
                runner_id = "?"
                try:
                    passed, detail, expect_test_failure, runner_id = run_test(chirp_bin, test_file)
                    if passed:
                        if expect_test_failure:
                            print_test_status("[XPASS]", name, COLOR_RED, " (Unexpectedly passed; please remove `expect_test_failure;)")
                            xpass_count += 1
                        else:
                            print_test_status("[PASS]", name, COLOR_GREEN)
                            passed_count += 1
                    else:
                        if expect_test_failure:
                            print_test_status("[XFAIL]", name, COLOR_YELLOW)
                            xfail_count += 1
                        else:
                            print_test_status("[FAIL]", name, COLOR_RED)
                            print("-" * 40)
                            print(detail)
                            print("-" * 40)
                            failed_count += 1
                except Exception as exc:
                    print_test_status("[ERROR]", name, COLOR_RED, f" raised an exception: {exc}")
                    failed_count += 1
    finally:
        with registry_lock:
            for path in temp_files_registry:
                if os.path.exists(path):
                    try:
                        os.remove(path)
                    except OSError:
                        pass
                
    print("=" * 60)
    print(f"Summary: {passed_count} passed, {xfail_count} expected failures (XFAIL)")
    if xpass_count > 0 or failed_count > 0:
        print(f"Errors: {failed_count} unexpected failures, {xpass_count} unexpected passes (XPASS)")
        sys.exit(1)
    else:
        sys.exit(0)

if __name__ == "__main__":
    main()
