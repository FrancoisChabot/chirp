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


def parse_args():
    parser = argparse.ArgumentParser(description="Run Chirp benchmarks against Python baselines.")
    parser.add_argument("benchmark", help="Benchmark directory name under benchmarks/")
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


def main():
    args = parse_args()
    if args.runs < 1:
        raise SystemExit("--runs must be at least 1")

    bench_dir = BENCH_ROOT / args.benchmark
    if not bench_dir.is_dir():
        raise SystemExit(f"Unknown benchmark: {args.benchmark}")

    chirp_script = require_file(bench_dir / "script.chirp")
    python_script = require_file(bench_dir / "script.py")

    chirp_cmd = [args.chirp, "--root-dir", args.root_dir, str(chirp_script)]
    python_cmd = [args.python, "-B", str(python_script)]
    python_env = os.environ.copy()
    python_env["PYTHONDONTWRITEBYTECODE"] = "1"

    print(f"benchmark: {args.benchmark}")
    print(f"runs:      {args.runs}")
    print()

    chirp_samples = []
    python_samples = []
    expected_stdout = None

    for _ in range(args.runs):
        elapsed, stdout = run_once("chirp", chirp_cmd, REPO_ROOT)
        chirp_samples.append(elapsed)
        if expected_stdout is None:
            expected_stdout = stdout
        elif stdout != expected_stdout:
            raise SystemExit("chirp output changed between runs")

        clean_python_cache(bench_dir)
        elapsed, stdout = run_once("python", python_cmd, REPO_ROOT, python_env)
        python_samples.append(elapsed)
        if stdout != expected_stdout:
            raise SystemExit(
                "python output did not match chirp output\n"
                f"chirp stdout:\n{expected_stdout}\n"
                f"python stdout:\n{stdout}"
            )

    print_summary("chirp", chirp_samples)
    print_summary("python", python_samples)

    chirp_median = summarize(chirp_samples)["median"]
    python_median = summarize(python_samples)["median"]
    if python_median > 0:
        print(f" ratio: {chirp_median / python_median:.2f}x chirp/python median")


if __name__ == "__main__":
    main()
