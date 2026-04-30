# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

import socketserver
import threading

import pytest

from pulsepins import PulsePins, PulsePinsCommandError, PulsePinsProtocolError, Timeline


class FakeScpiServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, address):
        super().__init__(address, FakeScpiHandler)
        self.commands = []
        self.errors = []
        self.check = False
        self.fail_stream = False


class FakeScpiHandler(socketserver.StreamRequestHandler):
    def handle(self):
        for raw in self.rfile:
            line = raw.decode("ascii").strip()
            self.server.commands.append(line)

            if line == "*IDN?":
                self._write("PulsePins,TEST")
            elif line == "*RST":
                self.server.check = False
            elif line == "*CLS":
                self.server.errors.clear()
            elif line == "CHECK ON":
                self.server.check = True
            elif line == "CHECK OFF":
                self.server.check = False
            elif line == "CHECK?":
                self._write("TRUE" if self.server.check else "FALSE")
            elif line.startswith("SEQ"):
                if "BAD" in line:
                    self.server.errors.append("Execution error: bad sequence")
                    self._write("ERROR")
                else:
                    self._write("LOADED")
            elif line == "STREAM":
                if self.server.fail_stream:
                    self.server.errors.append("Execution error: STREAM failed with rc=1")
                    self._write("FAILURE")
                else:
                    self._write("SUCCESS")
            elif line == "TEST1":
                self._write("SUCCESS")
            elif line == "SYST:ERR?":
                if self.server.errors:
                    self._write('100, "{}"'.format(self.server.errors.pop(0)))
                else:
                    self._write('0, "No error"')
            elif line == "DISCONNECT":
                return
            else:
                self.server.errors.append("Command error: unknown token")
                self._write("ERROR")

    def _write(self, text):
        self.wfile.write((text + "\n").encode("ascii"))
        self.wfile.flush()


@pytest.fixture
def scpi_server():
    server = FakeScpiServer(("127.0.0.1", 0))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=1)


def make_client(server):
    host, port = server.server_address
    return PulsePins(host, port=port, timeout=1.0)


def test_idn_and_check_roundtrip(scpi_server):
    with make_client(scpi_server) as pp:
        assert pp.idn() == "PulsePins,TEST"
        pp.check(True)
        assert pp.check_enabled() is True
        pp.check(False)
        assert pp.check_enabled() is False
        pp.reset()
        assert pp.check_enabled() is False


def test_load_sequence_flattens_multiline_text(scpi_server):
    with make_client(scpi_server) as pp:
        pp.load_sequence("d 10 0xff\n  d 5 0x00\n\tf\n")
        assert pp.stream() == "SUCCESS"

    assert "SEQ d 10 0xff d 5 0x00 f" in scpi_server.commands
    assert "STREAM" in scpi_server.commands


def test_load_sequence_accepts_timeline(scpi_server):
    timeline = Timeline()
    timeline.channel("q0", 0)
    timeline.pulse("q0", 0, 5)

    with make_client(scpi_server) as pp:
        pp.load(timeline, force_trigger=True)

    assert "SEQ d 5 0x1 f" in scpi_server.commands


def test_run_loads_checks_and_streams_timeline(scpi_server):
    timeline = Timeline()
    timeline.channel("q0", 0)
    timeline.pulse("q0", 0, 5)

    with make_client(scpi_server) as pp:
        assert pp.run(timeline, check=True, force_trigger=True) == "SUCCESS"

    check_index = scpi_server.commands.index("CHECK ON")
    seq_index = scpi_server.commands.index("SEQ d 5 0x1 f")
    stream_index = scpi_server.commands.index("STREAM")
    assert check_index < seq_index < stream_index


def test_load_sequence_error_includes_error_queue(scpi_server):
    with make_client(scpi_server) as pp:
        with pytest.raises(PulsePinsCommandError) as excinfo:
            pp.load_sequence("BAD")

    assert "SEQ returned 'ERROR'" in str(excinfo.value)
    assert "bad sequence" in str(excinfo.value)


def test_stream_failure_includes_error_queue(scpi_server):
    scpi_server.fail_stream = True
    with make_client(scpi_server) as pp:
        with pytest.raises(PulsePinsCommandError) as excinfo:
            pp.stream()

    assert "STREAM returned 'FAILURE'" in str(excinfo.value)
    assert "STREAM failed with rc=1" in str(excinfo.value)


def test_test1_runs_builtin_self_test(scpi_server):
    with make_client(scpi_server) as pp:
        assert pp.test1() == "SUCCESS"

    assert "TEST1" in scpi_server.commands


def test_rejects_oversize_sequence_line_before_send(scpi_server):
    with make_client(scpi_server) as pp:
        with pytest.raises(PulsePinsProtocolError) as excinfo:
            pp.load_sequence("x" * PulsePins.MAX_SCPI_LINE_BYTES)

    assert "accepts at most" in str(excinfo.value)
    assert not any(command.startswith("SEQ") for command in scpi_server.commands)


def test_system_error_parsing(scpi_server):
    scpi_server.errors.append("one")
    with make_client(scpi_server) as pp:
        assert pp.system_error() == (100, "one")
        assert pp.system_error() == (0, "No error")
