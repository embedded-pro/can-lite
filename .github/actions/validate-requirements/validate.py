#!/usr/bin/env python3
"""Validate YAML requirement files against a JSON schema."""

import argparse
import json
import re
import sys
from pathlib import Path

import yaml
from jsonschema import validate, ValidationError


TEST_DEFINITION = re.compile(r"\bTEST(?:_F)?\(\s*(\w+)\s*,\s*(\w+)\s*\)")


def defined_tests(sources_dir):
    tests = set()
    for source in sources_dir.glob("**/*.cpp"):
        for suite, name in TEST_DEFINITION.findall(source.read_text(errors="ignore")):
            tests.add(f"{suite}.{name}")
    return tests


def unresolved_tests(content, tests):
    return [
        f"{requirement['id']} names {test}"
        for requirement in content
        for test in requirement.get("tests", [])
        if test not in tests
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--schema", required=True, type=Path)
    parser.add_argument("--requirements-dir", required=True, type=Path)
    parser.add_argument("--sources-dir", type=Path)
    args = parser.parse_args()

    schema = json.loads(args.schema.read_text())
    tests = defined_tests(args.sources_dir) if args.sources_dir else None
    yaml_files = sorted(args.requirements_dir.glob("**/*.yaml"))
    failed = False

    for yaml_file in yaml_files:
        relative_path = yaml_file.relative_to(args.requirements_dir)
        content = yaml.safe_load(yaml_file.read_text())

        if content is None:
            print(f"⚠ {relative_path} (empty)")
            continue

        try:
            validate(content, schema)
        except ValidationError as e:
            print(f"✗ {relative_path}: {e.message}")
            failed = True
            continue

        missing = unresolved_tests(content, tests) if tests is not None else []
        for reference in missing:
            print(f"✗ {relative_path}: {reference}, which is not a test")
        failed = failed or bool(missing)

        if not missing:
            print(f"✓ {relative_path}")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
