# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

"""Small stdlib SCPI client for a remote ``ppscpi`` server.

This module is intentionally independent from the board-native nanobind
extension modules. It is meant for host-side scripts and Jupyter notebooks
that talk to a DE10-Nano running ``ppscpi`` over Ethernet.
"""

import socket
from typing import List, Optional, Tuple


class PulsePinsError(Exception):
    """Base class for host-side PulsePins client errors."""


class PulsePinsConnectionError(PulsePinsError):
    """Raised when the TCP connection to ``ppscpi`` fails."""


class PulsePinsProtocolError(PulsePinsError):
    """Raised when the SCPI transport or response format is invalid."""


class PulsePinsCommandError(PulsePinsError):
    """Raised when ``ppscpi`` reports a command failure."""


class PulsePins:
    """Line-oriented TCP client for ``ppscpi``.

    Parameters
    ----------
    host:
        Hostname or IP address of the board running ``ppscpi``.
    port:
        TCP port. ``ppscpi`` listens on 5025 by default.
    timeout:
        Socket timeout in seconds. ``None`` leaves the socket blocking.
    auto_connect:
        Connect immediately when the object is constructed.
    """

    MAX_SCPI_LINE_BYTES = 64 * 1024
    MAX_ERROR_DRAIN = 16

    def __init__(
        self,
        host: str,
        port: int = 5025,
        timeout: Optional[float] = 10.0,
        auto_connect: bool = True,
    ):
        self.host = host
        self.port = port
        self.timeout = timeout
        self._socket = None  # type: Optional[socket.socket]
        self._rx = bytearray()
        if auto_connect:
            self.connect()

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
        return False

    def connect(self):
        """Open the TCP connection if it is not already open."""
        if self._socket is not None:
            return self
        try:
            self._socket = socket.create_connection(
                (self.host, self.port), timeout=self.timeout
            )
        except OSError as exc:
            raise PulsePinsConnectionError(
                "Could not connect to ppscpi at {}:{}: {}".format(
                    self.host, self.port, exc
                )
            ) from exc
        self._rx.clear()
        return self

    def close(self):
        """Close the local TCP socket."""
        sock = self._socket
        self._socket = None
        self._rx.clear()
        if sock is not None:
            sock.close()

    def disconnect(self):
        """Ask ``ppscpi`` to close this session, then close the local socket."""
        if self._socket is not None:
            try:
                self.send("DISCONNECT")
            finally:
                self.close()

    def send(self, command: str) -> None:
        """Send a raw command that does not produce a response."""
        self._write_line(command)

    def query(self, command: str) -> str:
        """Send a raw command or query and return one response line."""
        self._write_line(command)
        return self._read_line()

    def idn(self) -> str:
        """Return the ``*IDN?`` identity string."""
        return self.query("*IDN?")

    def reset(self) -> None:
        """Clear the current SCPI session state."""
        self.send("*RST")

    def clear_status(self) -> None:
        """Clear SCPI status and the error queue."""
        self.send("*CLS")

    def load(self, sequence, **sequence_options) -> None:
        """Alias for ``load_sequence(...)``."""
        self.load_sequence(sequence, **sequence_options)

    def load_sequence(self, sequence, **sequence_options) -> None:
        """Parse and store a PulsePins text sequence in the remote session.

        ``ppscpi`` is line-oriented, while PulsePins sequence text is usually
        written over multiple lines. The payload is therefore normalized to a
        single whitespace-separated line before sending ``SEQ ...``. Objects
        with a ``to_sequence()`` method, such as ``Timeline``, are accepted too.
        Extra keyword arguments are passed to ``to_sequence(...)``.
        """
        if isinstance(sequence, str):
            if sequence_options:
                raise TypeError("sequence options require an object with to_sequence()")
        else:
            to_sequence = getattr(sequence, "to_sequence", None)
            if to_sequence is None:
                raise TypeError("sequence must be text or provide to_sequence()")
            sequence = to_sequence(**sequence_options)
        if not isinstance(sequence, str):
            raise TypeError("to_sequence() must return text")
        payload = " ".join(sequence.split())
        command = "SEQ" if not payload else "SEQ " + payload
        response = self.query(command)
        self._expect_response("SEQ", response, "LOADED")

    def check(self, enabled: bool) -> None:
        """Enable or disable readback checking for later ``stream()`` calls."""
        if not isinstance(enabled, bool):
            raise TypeError("enabled must be a bool")
        self.send("CHECK {}".format("ON" if enabled else "OFF"))

    def check_enabled(self) -> bool:
        """Return whether readback checking is enabled in this session."""
        response = self.query("CHECK?").upper()
        if response == "TRUE":
            return True
        if response == "FALSE":
            return False
        raise PulsePinsProtocolError("Unexpected CHECK? response: {!r}".format(response))

    def stream(self) -> str:
        """Stream the currently loaded sequence and return ``SUCCESS``.

        Raises ``PulsePinsCommandError`` when ``ppscpi`` returns ``FAILURE`` or
        another unexpected response. The error message includes drained
        ``SYST:ERR?`` records when available.
        """
        response = self.query("STREAM")
        self._expect_response("STREAM", response, "SUCCESS")
        return response

    def test1(self) -> str:
        """Run the built-in ``ppscpi`` short self-test and return ``SUCCESS``."""
        response = self.query("TEST1")
        self._expect_response("TEST1", response, "SUCCESS")
        return response

    def run(self, sequence, check: Optional[bool] = None, **sequence_options) -> str:
        """Load and stream a sequence in one call.

        ``check=None`` leaves the current session readback-check setting unchanged.
        Other keyword arguments are forwarded to ``load_sequence(...)``.
        """
        if check is not None:
            self.check(check)
        self.load_sequence(sequence, **sequence_options)
        return self.stream()

    def system_error(self) -> Tuple[int, str]:
        """Query one ``SYST:ERR?`` record as ``(code, message)``."""
        return self._parse_system_error(self.query("SYST:ERR?"))

    def errors(self) -> List[str]:
        """Drain and return all currently queued SCPI error messages."""
        out = []
        for _ in range(self.MAX_ERROR_DRAIN):
            code, message = self.system_error()
            if code == 0:
                return out
            out.append(message)
        raise PulsePinsProtocolError("SCPI error queue did not drain")

    def _expect_response(self, command: str, response: str, expected: str) -> None:
        if response == expected:
            return
        details = []
        try:
            details = self.errors()
        except PulsePinsError:
            pass
        message = "{} returned {!r}, expected {!r}".format(command, response, expected)
        if details:
            message += ": " + "; ".join(details)
        raise PulsePinsCommandError(message)

    def _write_line(self, command: str) -> None:
        if "\n" in command or "\r" in command:
            raise PulsePinsProtocolError("SCPI commands must be single-line strings")
        try:
            line = (command + "\n").encode("ascii")
        except UnicodeEncodeError as exc:
            raise PulsePinsProtocolError("SCPI commands must be ASCII") from exc
        if len(line) > self.MAX_SCPI_LINE_BYTES:
            raise PulsePinsProtocolError(
                "SCPI command is {} bytes including newline; ppscpi accepts at most {}".format(
                    len(line), self.MAX_SCPI_LINE_BYTES
                )
            )
        sock = self._require_socket()
        try:
            sock.sendall(line)
        except OSError as exc:
            self.close()
            raise PulsePinsConnectionError("Could not write to ppscpi: {}".format(exc)) from exc

    def _read_line(self) -> str:
        sock = self._require_socket()
        while True:
            newline = self._rx.find(b"\n")
            if newline >= 0:
                raw = bytes(self._rx[:newline])
                del self._rx[: newline + 1]
                if raw.endswith(b"\r"):
                    raw = raw[:-1]
                try:
                    return raw.decode("ascii")
                except UnicodeDecodeError as exc:
                    raise PulsePinsProtocolError("SCPI response was not ASCII") from exc
            if len(self._rx) >= self.MAX_SCPI_LINE_BYTES:
                raise PulsePinsProtocolError("SCPI response line is too long")
            try:
                chunk = sock.recv(4096)
            except OSError as exc:
                self.close()
                raise PulsePinsConnectionError("Could not read from ppscpi: {}".format(exc)) from exc
            if not chunk:
                self.close()
                raise PulsePinsConnectionError("ppscpi closed the connection")
            self._rx.extend(chunk)

    def _require_socket(self) -> socket.socket:
        if self._socket is None:
            self.connect()
        assert self._socket is not None
        return self._socket

    @staticmethod
    def _parse_system_error(response: str) -> Tuple[int, str]:
        code_text, sep, message = response.partition(",")
        if not sep:
            raise PulsePinsProtocolError(
                "Malformed SYST:ERR? response: {!r}".format(response)
            )
        try:
            code = int(code_text.strip())
        except ValueError as exc:
            raise PulsePinsProtocolError(
                "Malformed SYST:ERR? code: {!r}".format(response)
            ) from exc
        message = message.strip()
        if len(message) >= 2 and message[0] == '"' and message[-1] == '"':
            message = message[1:-1]
        return code, message
