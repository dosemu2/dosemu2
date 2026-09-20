"""IPX over UDP relay test.

dosemu's IPX can tunnel over UDP to a relay, speaking the protocol of
the DOSBox IPX server: a client registers by sending an IPX header with
the destination socket 2 and a zeroed destination address, the server
answers with the address it assigned, and from then on it routes the
packets by the IP address and port that are carried inside the IPX
header itself.

The relay below is that server, so the test needs neither DOSBox nor a
second dosemu. It registers dosemu, checks the broadcast dosemu sends,
plays a peer that broadcasts and unicasts back, and checks the unicast
answers - all four directions in one run.
"""

from shutil import copy
from subprocess import check_call
import socket
import struct
import threading

IPX_SOCKET = 0x4545             # the socket ipxtest.com uses
IPX_HDRLEN = 30
PAYLOAD = 8
PEER_NODE = bytes.fromhex("0a0b0c0d0e0f")
PEER_NET = bytes(4)
BROADCAST = b"\xff" * 6


def _header(dst_net, dst_node, dst_sock, src_net, src_node, src_sock, length):
    return (struct.pack(">HHBB", 0xffff, length, 0, 4) +
            dst_net + dst_node + struct.pack(">H", dst_sock) +
            src_net + src_node + struct.pack(">H", src_sock))


def _packet(dst_net, dst_node, src_net, src_node, tag):
    payload = b"IPX-" + tag + b"   "
    return _header(dst_net, dst_node, IPX_SOCKET, src_net, src_node,
                   IPX_SOCKET, IPX_HDRLEN + PAYLOAD) + payload


class Relay(threading.Thread):
    """The IPX over UDP server, and a peer to talk to."""

    def __init__(self):
        super().__init__(daemon=True)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.settimeout(0.2)
        self.port = self.sock.getsockname()[1]
        self.done = threading.Event()
        self.node = None            # the node we assigned to dosemu
        self.broadcasts = []        # tags dosemu broadcast
        self.unicasts = []          # tags dosemu sent to us
        self.faults = []            # anything malformed

    def _check(self, cond, what):
        if not cond:
            self.faults.append(what)
        return cond

    def _register(self, pkt, addr):
        node = socket.inet_aton(addr[0]) + struct.pack(">H", addr[1])
        self.node = node
        ack = _header(bytes(4), node, 2,
                      b"\0\0\0\1", bytes(4) + struct.pack(">H", self.port), 2,
                      IPX_HDRLEN)
        self.sock.sendto(ack, addr)

    def _data(self, pkt, addr):
        dst_node, dst_sock = pkt[10:16], struct.unpack(">H", pkt[16:18])[0]
        src_node, src_sock = pkt[22:28], struct.unpack(">H", pkt[28:30])[0]
        tag = pkt[IPX_HDRLEN + 4:IPX_HDRLEN + 5].decode("latin1")

        self._check(len(pkt) == IPX_HDRLEN + PAYLOAD,
                    "packet of %i bytes" % len(pkt))
        self._check(struct.unpack(">H", pkt[2:4])[0] == len(pkt),
                    "length field %i" % struct.unpack(">H", pkt[2:4])[0])
        self._check(dst_sock == IPX_SOCKET and src_sock == IPX_SOCKET,
                    "sockets %04x/%04x" % (dst_sock, src_sock))
        # the relay routes by what the header says the source is, so a
        # wrong one here would make broadcasts bounce back to dosemu
        self._check(src_node == self.node,
                    "source node %s, expected %s" % (src_node.hex(),
                                                     self.node.hex()))

        if dst_node == BROADCAST:
            self.broadcasts.append(tag)
            # answer as a peer would: a broadcast, then a unicast
            self.sock.sendto(_packet(bytes(4), BROADCAST, PEER_NET,
                                     PEER_NODE, b"B"), addr)
            self.sock.sendto(_packet(bytes(4), self.node, PEER_NET,
                                     PEER_NODE, b"C"), addr)
        elif dst_node == PEER_NODE:
            self.unicasts.append(tag)
        else:
            self.faults.append("unicast to unknown node %s" % dst_node.hex())

    def run(self):
        while not self.done.is_set():
            try:
                pkt, addr = self.sock.recvfrom(1500)
            except socket.timeout:
                continue
            if len(pkt) < IPX_HDRLEN:
                self.faults.append("runt packet of %i bytes" % len(pkt))
                continue
            if struct.unpack(">H", pkt[16:18])[0] == 2 and pkt[10:14] == bytes(4):
                self._register(pkt, addr)
            else:
                self._data(pkt, addr)
        self.sock.close()


def ipx_relay(self):
    if not getattr(self.__class__, 'ipxmade', False):
        check_call(["make", "--quiet", "-C", "test/ipx", "clean", "all"])
        self.__class__.ipxmade = True

    copy(self.topdir / "test" / "ipx" / "ipxtest.com",
         self.workdir / "ipxtest.com")

    relay = Relay()
    relay.start()

    self.mkfile("testit.bat", """\
emuipx -c localhost:%i
ipxtest A
rem end
""" % relay.port, newline="\r\n")

    try:
        results = self.runDosemu("testit.bat", timeout=60, config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_ipxsupport = (on)
""")
    finally:
        relay.done.set()
        relay.join(timeout=5)

    self.assertEqual(relay.faults, [])
    self.assertIsNotNone(relay.node, "dosemu did not register with the relay")

    # the address the relay handed out is the one DOS sees
    self.assertIn("ipxtest: node %s" % relay.node.hex().upper(), results)

    # broadcast out, and the two the peer sent coming in
    self.assertEqual(relay.broadcasts, ["A"])
    self.assertIn("rx B from %s" % PEER_NODE.hex().upper(), results)
    self.assertIn("rx C from %s" % PEER_NODE.hex().upper(), results)

    # both of those were answered by a unicast to the peer
    self.assertEqual(relay.unicasts, ["a", "a"])
    self.assertIn("ipxtest: done, rx 2", results)
