#!/usr/bin/env python3
"""Exercise the real worker's request boundary and observe normal process exit."""
import mmap
import os
import socket
import struct
import sys
import unittest

WORKER = sys.argv.pop(1)
CAPACITY = 1024 * 1024
MAGIC = 0x564C4D31
PACKET = struct.Struct("=4I")


class WorkerProtocolTests(unittest.TestCase):
    def exchange(self, source, packet):
        parent, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        memory_fd = os.memfd_create("vellum-test", os.MFD_CLOEXEC)
        os.ftruncate(memory_fd, CAPACITY)
        # Put both sources above the protocol descriptors before duplicating them.
        import fcntl
        child_fd = fcntl.fcntl(child.fileno(), fcntl.F_DUPFD_CLOEXEC, 10)
        high_memory_fd = fcntl.fcntl(memory_fd, fcntl.F_DUPFD_CLOEXEC, 10)
        try:
            with mmap.mmap(memory_fd, CAPACITY) as memory:
                memory[:len(source)] = source
                actions = [(os.POSIX_SPAWN_DUP2, child_fd, 3),
                           (os.POSIX_SPAWN_DUP2, high_memory_fd, 4)]
                pid = os.posix_spawn(WORKER, [WORKER], os.environ, file_actions=actions)
                child.close()
                os.close(child_fd)
                child_fd = None
                parent.settimeout(10)
                try:
                    parent.send(packet)
                    reply = parent.recv(PACKET.size + 1)
                    _, status = os.waitpid(pid, 0)
                    pid = None
                    return os.waitstatus_to_exitcode(status), reply, memory[:128]
                finally:
                    if pid is not None:
                        os.kill(pid, 9)
                        os.waitpid(pid, 0)
        finally:
            parent.close()
            child.close()
            if child_fd is not None:
                os.close(child_fd)
            os.close(high_memory_fd)
            os.close(memory_fd)

    def test_success_and_script_error(self):
        for source, expected_status, expected_text in (
                (b"6 * 7", 0, b"42"),
                (b"'a\\0b'", 0, b"a\0b"),
                ("'héllo 世界'".encode(), 0, "héllo 世界".encode()),
                (b"({toString(){throw Error('bad')}})", 1,
                 b"JavaScript result conversion failed"),
                (b"function {", 1, b"JavaScript compilation or execution failed"),
                (b"throw new Error('bad')", 1, b"JavaScript compilation or execution failed")):
            with self.subTest(source=source):
                code, reply, output = self.exchange(source, PACKET.pack(MAGIC, 1, len(source), 0))
                self.assertEqual(code, 0)
                self.assertEqual(PACKET.unpack(reply), (MAGIC, 1, len(expected_text), expected_status))
                self.assertEqual(output[:len(expected_text)], expected_text)

    def test_invalid_requests_are_rejected(self):
        packets = (PACKET.pack(0, 1, 0, 0), PACKET.pack(MAGIC, 2, 0, 0),
                   PACKET.pack(MAGIC, 1, CAPACITY + 1, 0),
                   PACKET.pack(MAGIC, 1, 0, 1), PACKET.pack(MAGIC, 1, 0, 2),
                   b"short", PACKET.pack(MAGIC, 1, 0, 0) + b"extra")
        for packet in packets:
            with self.subTest(packet=packet):
                code, reply, _ = self.exchange(b"", packet)
                self.assertEqual(code, 2)
                self.assertEqual(reply, b"")

    def test_maximum_request_and_oversized_result(self):
        source = b"1" + b" " * (CAPACITY - 1)
        code, reply, output = self.exchange(source, PACKET.pack(MAGIC, 1, len(source), 0))
        self.assertEqual(code, 0)
        self.assertEqual(PACKET.unpack(reply), (MAGIC, 1, 1, 0))
        self.assertEqual(output[:1], b"1")

        source = b"'x'.repeat(1048577)"
        code, reply, _ = self.exchange(source, PACKET.pack(MAGIC, 1, len(source), 0))
        self.assertEqual(code, 2)
        self.assertEqual(reply, b"")


if __name__ == "__main__":
    unittest.main()
