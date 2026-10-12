"""Packet capture through a memory-mapped RX ring (TPACKET_V2): the kernel COPIES each packet into the ring at the moment it passes the tap.
A plain AF_PACKET socket only queues a clone that shares the packet's data; if the next hop rewrites the header in place (as an SRv6 End does),
a late recvfrom() would show the rewritten bytes. The ring avoids that."""
import mmap, socket, struct, time

SOL_PACKET, PACKET_RX_RING, PACKET_VERSION, TPACKET_V2 = 263, 5, 10, 1
BLOCK, FRAMES, FRAME = 1 << 16, 64, 1 << 11                                 # 64 frames of 2 KiB in one 128 KiB-ish ring

class Tap:
    def __init__(self, iface):
        self.s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.ntohs(3))
        self.s.setsockopt(SOL_PACKET, PACKET_VERSION, TPACKET_V2)
        nblocks = FRAMES * FRAME // BLOCK
        self.s.setsockopt(SOL_PACKET, PACKET_RX_RING, struct.pack("IIII", BLOCK, nblocks, FRAME, FRAMES))
        self.ring = mmap.mmap(self.s.fileno(), BLOCK * nblocks); self.s.bind((iface, 0)); self.i = 0

    def packets(self, timeout):
        """Yield (pkttype, raw_bytes, timestamp); pkttype 0 = received from the wire, 4 = sent by this host."""
        end = time.time() + timeout
        while time.time() < end:
            off = self.i * FRAME
            status = struct.unpack_from("I", self.ring, off)[0]
            if status & 1:                                                  # TP_STATUS_USER: the kernel filled this slot
                snaplen, mac = struct.unpack_from("I", self.ring, off + 8)[0], struct.unpack_from("H", self.ring, off + 12)[0]
                pkttype = self.ring[off + 42]; sec, nsec = struct.unpack_from("II", self.ring, off + 16)
                yield pkttype, bytes(self.ring[off + mac: off + mac + snaplen]), sec + nsec / 1e9
                struct.pack_into("I", self.ring, off, 0); self.i = (self.i + 1) % FRAMES        # give the slot back to the kernel
            else:
                time.sleep(0.002)
