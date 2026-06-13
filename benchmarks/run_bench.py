#!/usr/bin/env python3
import argparse
import os
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path


BENCH_ROOT = Path(__file__).resolve().parent
REPO_ROOT = BENCH_ROOT.parent
DEFAULT_CHIRP = REPO_ROOT / "build" / "interpreter" / "chirp"
DEFAULT_CHIRP_ROOT = REPO_ROOT / "lib" / "chirp"


def benchmark_names():
    names = []
    for child in sorted(BENCH_ROOT.iterdir()):
        if not child.is_dir():
            continue
        if (child / "script.chirp").exists() and (child / "script.py").exists():
            names.append(child.name)
    return names


def parse_args():
    parser = argparse.ArgumentParser(description="Run Chirp benchmarks against Python baselines.")
    parser.add_argument(
        "benchmark",
        help='Benchmark directory name under benchmarks/, or "all" to run the full suite.',
    )
    parser.add_argument("--list", action="store_true", help="List available benchmarks and exit.")
    parser.add_argument("--runs", type=int, default=5, help="Cold process runs per implementation. Defaults to 5.")
    parser.add_argument("--chirp", default=os.environ.get("CHIRP_BIN", str(DEFAULT_CHIRP)), help="Path to chirp executable.")
    parser.add_argument("--python", default=sys.executable, help="Path to python executable.")
    parser.add_argument("--root-dir", default=str(DEFAULT_CHIRP_ROOT), help="Chirp root containing boot/ and std/.")
    return parser.parse_args()


def require_file(path):
    if not path.exists():
        raise SystemExit(f"Missing benchmark file: {path}")
    return path


def clean_python_cache(bench_dir):
    cache_dir = bench_dir / "__pycache__"
    if cache_dir.exists():
        shutil.rmtree(cache_dir)


def run_once(name, command, cwd, env=None):
    start = time.perf_counter_ns()
    result = subprocess.run(
        command,
        cwd=cwd,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    elapsed_ns = time.perf_counter_ns() - start
    if result.returncode != 0:
        raise SystemExit(
            f"{name} failed with exit code {result.returncode}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return elapsed_ns / 1_000_000_000, result.stdout


def summarize(samples):
    return {
        "min": min(samples),
        "median": statistics.median(samples),
        "mean": statistics.mean(samples),
    }


def format_seconds(value):
    return f"{value:.6f}s"


def print_summary(name, samples):
    summary = summarize(samples)
    print(
        f"{name:>6}: "
        f"min {format_seconds(summary['min'])}  "
        f"median {format_seconds(summary['median'])}  "
        f"mean {format_seconds(summary['mean'])}"
    )


def run_benchmark(benchmark, args):
    bench_dir = BENCH_ROOT / benchmark
    if not bench_dir.is_dir():
        raise SystemExit(f"Unknown benchmark: {benchmark}")

    chirp_script = require_file(bench_dir / "script.chirp")
    python_script = require_file(bench_dir / "script.py")

    interpreter_cmd = [args.chirp, "--root-dir", args.root_dir, str(chirp_script)]
    vm_cmd = [args.chirp, "--vm", "--root-dir", args.root_dir, str(chirp_script)]
    python_cmd = [args.python, "-B", str(python_script)]
    python_env = os.environ.copy()
    python_env["PYTHONDONTWRITEBYTECODE"] = "1"

    print(f"benchmark: {benchmark}")
    print(f"runs:      {args.runs}")
    print()

    interpreter_samples = []
    vm_samples = []
    python_samples = []
    expected_stdout = None

    for _ in range(args.runs):
        elapsed, stdout = run_once("interpreter", interpreter_cmd, REPO_ROOT)
        interpreter_samples.append(elapsed)
        if expected_stdout is None:
            expected_stdout = stdout
        elif stdout != expected_stdout:
            raise SystemExit("interpreter output changed between runs")

        elapsed, stdout = run_once("vm", vm_cmd, REPO_ROOT)
        vm_samples.append(elapsed)
        if stdout != expected_stdout:
            raise SystemExit(
                "vm output did not match interpreter output\n"
                f"expected stdout:\n{expected_stdout}\n"
                f"vm stdout:\n{stdout}"
            )

        clean_python_cache(bench_dir)
        elapsed, stdout = run_once("python", python_cmd, REPO_ROOT, python_env)
        python_samples.append(elapsed)
        if stdout != expected_stdout:
            raise SystemExit(
                "python output did not match expected output\n"
                f"expected stdout:\n{expected_stdout}\n"
                f"python stdout:\n{stdout}"
            )

    print_summary("interp", interpreter_samples)
    print_summary("vm", vm_samples)
    print_summary("python", python_samples)

    interpreter_median = summarize(interpreter_samples)["median"]
    vm_median = summarize(vm_samples)["median"]
    python_median = summarize(python_samples)["median"]
    
    if python_median > 0:
        print(f" ratio: {interpreter_median / python_median:.2f}x interpreter/python median")
        print(f" ratio: {vm_median / python_median:.2f}x vm/python median")
    if vm_median > 0:
        print(f" ratio: {interpreter_median / vm_median:.2f}x interpreter/vm median")

    return {
        "benchmark": benchmark,
        "interpreter_median": interpreter_median,
        "vm_median": vm_median,
        "python_median": python_median,
    }


def main():
    args = parse_args()
    names = benchmark_names()

    if args.list:
        for name in names:
            print(name)
        return

    if args.runs < 1:
        raise SystemExit("--runs must be at least 1")

    if args.benchmark == "all":
        results = []
        for i, benchmark in enumerate(names):
            if i > 0:
                print()
            results.append(run_benchmark(benchmark, args))

        if len(results) > 1:
            print()
            print("suite summary:")
            for result in results:
                interp_vs_vm = result["interpreter_median"] / result["vm_median"] if result["vm_median"] > 0 else float("inf")
                vm_vs_python = result["vm_median"] / result["python_median"] if result["python_median"] > 0 else float("inf")
                print(
                    f"{result['benchmark']:>12}: "
                    f"vm/python {vm_vs_python:.2f}x  "
                    f"interpreter/vm {interp_vs_vm:.2f}x"
                )
        return

    run_benchmark(args.benchmark, args)


if __name__ == "__main__":
    main()
