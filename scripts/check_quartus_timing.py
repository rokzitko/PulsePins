from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


REQUIRED_STREAMER_CLOCKS = ("STREAMER_FROM_INT", "STREAMER_FROM_CLEAN")
FORBIDDEN_STREAMER_CLOCKS = ("STREAMER_FROM_EXT",)


@dataclass
class Finding:
    category: str
    message: str
    path: Path | None = None
    line: int | None = None

    def format(self, root: Path) -> str:
        location = ""
        if self.path is not None:
            try:
                rel = self.path.relative_to(root)
            except ValueError:
                rel = self.path
            location = str(rel)
            if self.line is not None:
                location += f":{self.line}"
            location += ": "
        return f"[{self.category}] {location}{self.message}"


def read_lines(path: Path) -> list[str]:
    try:
        return path.read_text(errors="replace").splitlines()
    except FileNotFoundError:
        return []


def require_file(root: Path, name: str, findings: list[Finding]) -> Path:
    path = root / name
    if not path.exists():
        findings.append(Finding("missing-file", f"required report file is missing: {name}"))
    return path


def scan_report_messages(root: Path, names: list[str], findings: list[Finding]) -> None:
    ignored_sdc = re.compile(r"Ignored .+ at (?:.*[/\\])?pulsepins\.sdc\(\d+\)")
    pll_cross_check = "PLL cross checking found inconsistent PLL clock settings"

    for name in names:
        path = root / name
        if not path.exists():
            continue
        for line_no, line in enumerate(read_lines(path), 1):
            if ignored_sdc.search(line):
                findings.append(Finding("sdc-ignored", line.strip(), path, line_no))
            if pll_cross_check in line:
                findings.append(Finding("pll-clock", line.strip(), path, line_no))


def check_flow_status(root: Path, findings: list[Finding]) -> None:
    path = root / "pulsepins.flow.rpt"
    if not path.exists():
        return
    for line_no, line in enumerate(read_lines(path), 1):
        if "Flow Status" in line:
            if "Successful" not in line:
                findings.append(Finding("flow", line.strip(), path, line_no))
            return
    findings.append(Finding("flow", "Flow Status line not found", path))


def check_streamer_clocks(root: Path, findings: list[Finding]) -> None:
    path = root / "pulsepins.sta.rpt"
    if not path.exists():
        return
    text = path.read_text(errors="replace")
    for clock in REQUIRED_STREAMER_CLOCKS:
        if clock not in text:
            findings.append(Finding("streamer-clock", f"required generated clock is missing from STA report: {clock}", path))
    for clock in FORBIDDEN_STREAMER_CLOCKS:
        if clock in text:
            findings.append(Finding("streamer-clock", f"obsolete generated clock name still appears in STA report: {clock}", path))


def parse_sta_summary(root: Path, findings: list[Finding]) -> None:
    path = root / "pulsepins.sta.summary"
    if not path.exists():
        return
    current_type = None
    current_type_line = None
    for line_no, line in enumerate(read_lines(path), 1):
        if line.startswith("Type  :"):
            current_type = line.split(":", 1)[1].strip()
            current_type_line = line_no
        elif line.startswith("Slack :") and current_type is not None:
            value_text = line.split(":", 1)[1].strip()
            try:
                slack = float(value_text)
            except ValueError:
                findings.append(Finding("timing", f"could not parse slack value {value_text!r} for {current_type}", path, line_no))
                continue
            if slack < 0:
                findings.append(Finding("timing", f"negative slack {slack:.3f} ns for {current_type}", path, current_type_line))


def parse_unconstrained_summary(root: Path, findings: list[Finding]) -> None:
    path = root / "pulsepins.sta.rpt"
    if not path.exists():
        return

    rows = {
        "Illegal Clocks",
        "Unconstrained Clocks",
        "Unconstrained Input Ports",
        "Unconstrained Input Port Paths",
        "Unconstrained Output Ports",
        "Unconstrained Output Port Paths",
    }
    unconstrained_clock_names: list[str] = []
    in_clock_status = False

    for line_no, line in enumerate(read_lines(path), 1):
        stripped = line.strip()
        if stripped.startswith("; Clock Status Summary"):
            in_clock_status = True
            continue
        if in_clock_status and stripped.startswith(";") and stripped.endswith(";") and "Unconstrained" in stripped:
            parts = [part.strip() for part in stripped.strip(";").split(";")]
            if len(parts) >= 4 and parts[-1] == "Unconstrained":
                unconstrained_clock_names.append(parts[0])
        elif in_clock_status and stripped.startswith("+") and unconstrained_clock_names:
            in_clock_status = False

        parts = [part.strip() for part in stripped.strip(";").split(";")]
        if len(parts) == 3 and parts[0] in rows:
            try:
                setup = int(parts[1])
                hold = int(parts[2])
            except ValueError:
                findings.append(Finding("unconstrained", f"could not parse unconstrained-path row: {stripped}", path, line_no))
                continue
            if setup or hold:
                findings.append(Finding("unconstrained", f"{parts[0]}: setup={setup}, hold={hold}", path, line_no))

    for name in unconstrained_clock_names[:10]:
        findings.append(Finding("unconstrained-clock", f"clock is unconstrained: {name}", path))
    if len(unconstrained_clock_names) > 10:
        findings.append(Finding("unconstrained-clock", f"{len(unconstrained_clock_names) - 10} more unconstrained clocks omitted", path))


def main() -> int:
    parser = argparse.ArgumentParser(description="Check Quartus reports for PulsePins timing signoff issues.")
    parser.add_argument("--root", default=".", help="Quartus project/report directory, default: current directory")
    parser.add_argument(
        "--sdc-only",
        action="store_true",
        help="only check project SDC integrity and streamer generated-clock presence",
    )
    args = parser.parse_args()

    root = Path(args.root).resolve()
    findings: list[Finding] = []

    required_reports = ("pulsepins.sta.rpt",) if args.sdc_only else (
        "pulsepins.flow.rpt",
        "pulsepins.fit.rpt",
        "pulsepins.sta.rpt",
        "pulsepins.sta.summary",
    )
    for name in required_reports:
        require_file(root, name, findings)

    scan_report_messages(root, ["build-log-compile", "pulsepins.fit.rpt", "pulsepins.sta.rpt"], findings)
    check_streamer_clocks(root, findings)

    if not args.sdc_only:
        check_flow_status(root, findings)
        parse_sta_summary(root, findings)
        parse_unconstrained_summary(root, findings)

    if findings:
        print("Quartus timing/report checks failed:", file=sys.stderr)
        for finding in findings:
            print("  " + finding.format(root), file=sys.stderr)
        return 1

    print("Quartus timing/report checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
