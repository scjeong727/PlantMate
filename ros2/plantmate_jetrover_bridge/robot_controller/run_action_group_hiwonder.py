#!/usr/bin/env python3
"""
Run one Hiwonder .d6a action group by name or by file path.

This script targets Hiwonder arm_pc style runtimes where ActionGroupController
is available from action_group_controller.py.
"""

import argparse
import os
import shutil
import sys


ACTION_GROUP_DIRS = [
    "/home/ubuntu/software/arm_pc/ActionGroups",
    "/home/ubuntu/share/arm_pc/ActionGroups",
    "/home/hiwonder/software/arm_pc/ActionGroups",
    "/home/ubuntu/arm_pc/ActionGroups",
    "/home/hiwonder/arm_pc/ActionGroups",
    "/home/ubuntu/ros2_ws/src/driver/servo_controller/servo_controller/ActionGroups",

]

ARM_PC_DIRS = [
    "/home/ubuntu/software/arm_pc",       
    "/home/ubuntu/share/arm_pc",
    "/home/hiwonder/software/arm_pc",
    "/home/ubuntu/arm_pc",
    "/home/hiwonder/arm_pc",
    "/home/ubuntu/ros2_ws/src/driver/servo_controller/servo_controller",

]

MODULE_CANDIDATES = [
    "action_group_controller.py",
]


def parse_args():
    parser = argparse.ArgumentParser(description="Run Hiwonder action group")
    parser.add_argument(
        "action",
        help="Action file path (*.d6a) or action name (without .d6a)",
    )
    parser.add_argument(
        "--count",
        type=int,
        default=1,
        help="How many times to run action group (default: 1)",
    )
    return parser.parse_args()


def first_existing(paths):
    for path in paths:
        if os.path.isdir(path):
            return path
    return paths[0]


def import_controller():
    for path in ARM_PC_DIRS:
        if os.path.isdir(path) and path not in sys.path:
            sys.path.insert(0, path)
    try:
        from action_group_controller import ActionGroupController
        return ActionGroupController
    except Exception as exc:
        found_path = _find_module_path()
        print(
            "[action_group] failed to import ActionGroupController "
            f"from arm_pc runtime: {exc}",
            file=sys.stderr,
        )
        if found_path:
            print(
                "[action_group] found action_group_controller candidate at: "
                f"{found_path}",
                file=sys.stderr,
            )
        else:
            print(
                "[action_group] no action_group_controller module found in common paths.",
                file=sys.stderr,
            )
        print(
            "[action_group] check this VM has Hiwonder arm runtime. "
            "If not, run this command on the JetRover/arm controller machine.",
            file=sys.stderr,
        )
        return None


def _find_module_path():
    for base in ARM_PC_DIRS:
        if not os.path.isdir(base):
            continue
        for root, _dirs, files in os.walk(base):
            for filename in MODULE_CANDIDATES:
                if filename in files:
                    return os.path.join(root, filename)
    return None


def ensure_action_file(action_arg, action_groups_dir):
    if os.path.sep in action_arg or action_arg.endswith(".d6a"):
        src_file = action_arg
        if not os.path.isfile(src_file):
            raise FileNotFoundError(f"action file not found: {src_file}")
        action_name = os.path.splitext(os.path.basename(src_file))[0]
        dst_file = os.path.join(action_groups_dir, f"{action_name}.d6a")
        os.makedirs(action_groups_dir, exist_ok=True)
        if os.path.realpath(src_file) != os.path.realpath(dst_file):
            shutil.copy2(src_file, dst_file)
            print(f"[action_group] copied action file -> {dst_file}")
        return action_name, dst_file

    action_name = os.path.splitext(os.path.basename(action_arg))[0]
    dst_file = os.path.join(action_groups_dir, f"{action_name}.d6a")
    if not os.path.isfile(dst_file):
        raise FileNotFoundError(
            f"action file not found in ActionGroups: {dst_file}"
        )
    return action_name, dst_file


def main():
    args = parse_args()
    count = max(1, int(args.count))
    action_groups_dir = first_existing(ACTION_GROUP_DIRS)

    try:
        action_name, action_file = ensure_action_file(args.action, action_groups_dir)
    except Exception as exc:
        print(f"[action_group] {exc}", file=sys.stderr)
        return 2

    print(
        f"[action_group] prepared action_name={action_name} "
        f"file={action_file} count={count}"
    )

    controller_cls = import_controller()
    if controller_cls is None:
        return 3

    controller = None
    create_errors = []
    for use_ros in (True, False):
        try:
            controller = controller_cls(use_ros=use_ros)
            print(f"[action_group] controller initialized (use_ros={use_ros})")
            break
        except Exception as exc:
            create_errors.append(f"use_ros={use_ros}: {exc}")

    if controller is None:
        print(
            "[action_group] failed to initialize ActionGroupController: "
            + " | ".join(create_errors),
            file=sys.stderr,
        )
        return 4

    run_method = None
    if hasattr(controller, "runAction"):
        run_method = controller.runAction
    elif hasattr(controller, "run_action"):
        run_method = controller.run_action
    else:
        print(
            "[action_group] controller has no run method "
            "(expected runAction or run_action)",
            file=sys.stderr,
        )
        return 5

    for idx in range(count):
        print(f"[action_group] run {idx + 1}/{count}: {action_name}")
        run_method(action_name)

    print("[action_group] done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

