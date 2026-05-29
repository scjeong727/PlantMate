#!/usr/bin/env python3
import argparse
import subprocess
import sys
from pathlib import Path


def run_step(script_dir: Path, script_name: str, args=None, timeout_sec: int = 120):
    script_path = script_dir / script_name
    if not script_path.exists():
        raise FileNotFoundError(f"missing script: {script_path}")

    cmd = [sys.executable, str(script_path)]
    if args:
        cmd.extend(args)

    print(f"[water_sequence] start: {' '.join(cmd)}", flush=True)
    subprocess.run(cmd, cwd=str(script_dir), check=True, timeout=timeout_sec)
    print(f"[water_sequence] done: {script_name}", flush=True)


def parse_args():
    parser = argparse.ArgumentParser(description="Run full water sequence")
    parser.add_argument("--x", type=float, default=0.0, help="Target X for move-in")
    parser.add_argument("--y", type=float, default=0.0, help="Target Y for move-in")
    parser.add_argument("--home-x", type=float, default=None, help="Home X for move-back")
    parser.add_argument("--home-y", type=float, default=None, help="Home Y for move-back")
    return parser.parse_args()


def main():
    args = parse_args()
    script_dir = Path(__file__).resolve().parent

    home_x = -args.x if args.home_x is None else args.home_x
    home_y = -args.y if args.home_y is None else args.home_y

    run_step(script_dir, "pick_demo.py", timeout_sec=120)
    run_step(
        script_dir,
        "run_move_demo.py",
        ["--x", str(args.x), "--y", str(args.y)],
        timeout_sec=150,
    )
    run_step(script_dir, "run_watering_demo.py", timeout_sec=60)
    run_step(
        script_dir,
        "run_move_demo.py",
        ["--x", str(home_x), "--y", str(home_y)],
        timeout_sec=150,
    )
    run_step(script_dir, "run_watering_end_demo.py", timeout_sec=120)


if __name__ == "__main__":
    main()

