#!/usr/bin/env python3
import argparse
import os
import sys


def merge(boot_path, updater_path, app_path, output_path, updater_offset, app_offset):
    with open(boot_path, "rb") as f:
        bl = f.read()

    with open(updater_path, "rb") as f:
        updater = f.read()

    with open(app_path, "rb") as f:
        app = f.read()

    if len(bl) > updater_offset:
        raise RuntimeError("Bootloader too large")

    if len(updater) > (app_offset - updater_offset):
        raise RuntimeError("Updater too large")

    pad1 = b'\xFF' * (updater_offset - len(bl))
    pad2 = b'\xFF' * (app_offset - updater_offset - len(updater))

    merged = bl + pad1 + updater + pad2 + app

    with open(output_path, "wb") as f:
        f.write(merged)

    print(f"MERGED IMAGE: {output_path}")
    print(f"  BOOTLOADER : {len(bl)} bytes")
    print(f"  UPDATER    : {len(updater)} bytes")
    print(f"  APP        : {len(app)} bytes")
    print(f"  TOTAL      : {len(merged)} bytes")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("bootloader")
    parser.add_argument("updater")
    parser.add_argument("app")
    parser.add_argument("output")
    parser.add_argument("--updater-offset", type=lambda x: int(x, 0), required=True)
    parser.add_argument("--app-offset", type=lambda x: int(x, 0), required=True)
    args = parser.parse_args()

    merge(
        args.bootloader,
        args.updater,
        args.app,
        args.output,
        args.updater_offset,
        args.app_offset
    )


if __name__ == "__main__":
    main()