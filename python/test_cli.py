# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

import pulsepins.cli as cli


def test_timeline_preview_cli_writes_sidecar_files(tmp_path, capsys):
    svg_path = tmp_path / "timeline.svg"
    csv_path = tmp_path / "timeline.csv"
    draft_path = tmp_path / "timeline.json"
    vcd_path = tmp_path / "timeline.vcd"

    cli.timeline_preview_main(
        [
            "--svg",
            str(svg_path),
            "--csv",
            str(csv_path),
            "--draft",
            str(draft_path),
            "--vcd",
            str(vcd_path),
        ]
    )

    output = capsys.readouterr().out
    assert "d 1000 0x4" in output
    assert "final 0x0\nf\n" in output
    assert svg_path.read_text(encoding="utf-8").startswith(
        '<svg xmlns="http://www.w3.org/2000/svg"'
    )
    assert csv_path.read_text(encoding="utf-8").startswith(
        "channel,bit,start,duration,color\n"
    )
    assert '"format": "pulsepins.timeline"' in draft_path.read_text(
        encoding="utf-8"
    )
    assert "$timescale 10ns $end" in vcd_path.read_text(encoding="utf-8")


def test_timeline_sweep_cli_dry_run(capsys):
    cli.timeline_sweep_main(["--dry-run", "--delays-us", "0", "5"])

    output = capsys.readouterr().out
    assert "# camera delay: 0.0 us" in output
    assert "# camera delay: 5.0 us" in output
    assert output.count("final 0x0\n") == 2
    assert output.count("f\n") == 2


def test_timeline_stream_cli_uploads_final(monkeypatch, capsys):
    calls = []

    class FakePulsePins:
        def __init__(self, host, port=5025):
            calls.append(("init", host, port))

        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, tb):
            return False

        def idn(self):
            return "PulsePins,TEST"

        def streamer_clock_hz(self):
            calls.append(("streamer_clock_hz",))
            return 50_000_000.0

        def reset(self):
            calls.append(("reset",))

        def check(self, enabled):
            calls.append(("check", enabled))

        def load(self, sequence, **options):
            calls.append(("load", sequence.to_sequence(**options), options))

        def stream(self):
            return "SUCCESS"

    monkeypatch.setattr(cli, "PulsePins", FakePulsePins)
    cli.timeline_stream_main(["board.local", "--port", "1234", "--print-sequence"])

    output = capsys.readouterr().out
    assert "d 500 0x4\n" in output
    assert "final 0x0\nf\n" in output
    assert ("streamer_clock_hz",) in calls
    load_calls = [call for call in calls if call[0] == "load"]
    assert len(load_calls) == 1
    assert load_calls[0][2] == {"force_trigger": True, "include_final": True}
    assert "d 500 0x4\n" in load_calls[0][1]
    assert "final 0x0\nf\n" in load_calls[0][1]


def test_timeline_stream_cli_clock_override_skips_query(monkeypatch):
    calls = []

    class FakePulsePins:
        def __init__(self, host, port=5025):
            calls.append(("init", host, port))

        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, tb):
            return False

        def idn(self):
            return "PulsePins,TEST"

        def streamer_clock_hz(self):
            raise AssertionError("unexpected board clock query")

        def reset(self):
            calls.append(("reset",))

        def check(self, enabled):
            calls.append(("check", enabled))

        def load(self, sequence, **options):
            calls.append(("load", sequence.to_sequence(**options), options))

        def stream(self):
            return "SUCCESS"

    monkeypatch.setattr(cli, "PulsePins", FakePulsePins)
    cli.timeline_stream_main(["board.local", "--clock-hz", "50000000"])

    load_calls = [call for call in calls if call[0] == "load"]
    assert len(load_calls) == 1
    assert "d 500 0x4\n" in load_calls[0][1]


def test_timeline_sweep_cli_live_uses_board_clock(monkeypatch, capsys):
    calls = []

    class FakePulsePins:
        def __init__(self, host, port=5025):
            calls.append(("init", host, port))

        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, tb):
            return False

        def idn(self):
            return "PulsePins,TEST"

        def streamer_clock_hz(self):
            calls.append(("streamer_clock_hz",))
            return 50_000_000.0

        def reset(self):
            calls.append(("reset",))

        def run(self, sequence, check=None, **options):
            calls.append(("run", sequence.to_sequence(**options), check, options))
            return "SUCCESS"

    monkeypatch.setattr(cli, "PulsePins", FakePulsePins)
    cli.timeline_sweep_main(["board.local", "--port", "1234", "--delays-us", "0"])

    output = capsys.readouterr().out
    assert "camera delay: 0.0 us" in output
    assert ("streamer_clock_hz",) in calls
    run_calls = [call for call in calls if call[0] == "run"]
    assert len(run_calls) == 1
    assert "d 500 0x4\n" in run_calls[0][1]
    assert run_calls[0][2] is False
    assert run_calls[0][3] == {"force_trigger": True, "include_final": True}


def test_notebook_workflow_cli_dry_run(tmp_path, capsys):
    output_dir = tmp_path / "previews"

    cli.notebook_workflow_main(
        ["--output-dir", str(output_dir), "--delays-us", "0", "5"]
    )

    output = capsys.readouterr().out
    assert "python3 -m pip install -e /path/to/PulsePins/python" in output
    assert "# Dry run using clock_hz=100000000" in output
    assert "# Generated sequence:" in output
    assert "# Sweep camera delay: 5.0 us" in output
    assert output.count("final 0x0\n") == 3
    assert (output_dir / "timeline.svg").exists()
    assert (output_dir / "timeline.csv").exists()
    assert (output_dir / "timeline.json").exists()
    assert (output_dir / "timeline.vcd").exists()


def test_ppscpi_check_cli_reports_identity(monkeypatch, capsys):
    calls = []

    class FakePulsePins:
        def __init__(self, host, port=5025):
            calls.append((host, port))

        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, tb):
            return False

        def idn(self):
            return "PulsePins,TEST"

        def streamer_clock_hz(self):
            return 100_000_000.0

        def check_enabled(self):
            return True

        def errors(self):
            return []

        def test1(self):
            return "SUCCESS"

    monkeypatch.setattr(cli, "PulsePins", FakePulsePins)
    cli.ppscpi_check_main(["board.local", "--port", "1234", "--self-test"])

    assert calls == [("board.local", 1234)]
    output = capsys.readouterr().out
    assert "PulsePins,TEST" in output
    assert "STREAMER_CLOCK_HZ 100000000" in output
    assert "CHECK ON" in output
    assert "No SCPI errors" in output
    assert "TEST1 SUCCESS" in output
