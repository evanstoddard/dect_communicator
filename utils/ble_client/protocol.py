"""
Protocol definitions matching the firmware headers:
  - messaging_service_protocol.h  (BLE service framing layer)
  - messaging_endpoint_protocol.h (application-level messages)
"""

import struct
import uuid as _uuid
from dataclasses import dataclass

# ---------------------------------------------------------------------------
# BLE Service UUIDs
# ---------------------------------------------------------------------------
MESSAGING_SERVICE_UUID = "c1534fa3-5211-4e32-a176-d1af04513305"
MESSAGING_SERVICE_DATA_CHAR_UUID = "c1534fa4-5211-4e32-a176-d1af04513305"

# ---------------------------------------------------------------------------
# Service frame types  (messaging_service_frame_type_t)
# ---------------------------------------------------------------------------
FRAME_TYPE_ACK = 0x00
FRAME_TYPE_PAYLOAD = 0x01
FRAME_TYPE_RESET = 0xFF

# ---------------------------------------------------------------------------
# Service frame sizes
# ---------------------------------------------------------------------------
# Header layout: version(u8) frame_type(u8) frag_total(u8) frag_idx(u8) seq_id(u16 LE)
FRAME_HEADER_FMT = "<BBBBH"
FRAME_HEADER_SIZE = struct.calcsize(FRAME_HEADER_FMT)  # 6

MAX_ATT_PAYLOAD = 251  # Assumes negotiated MTU of 254
MAX_FRAME_PAYLOAD = MAX_ATT_PAYLOAD - FRAME_HEADER_SIZE  # 245

# ---------------------------------------------------------------------------
# Messaging endpoint constants
# ---------------------------------------------------------------------------
MSG_TYPE_TEXT = 0x00

ENDPOINT_HEADER_FMT = "<BB"  # version(u8) msg_type(u8)
ENDPOINT_HEADER_SIZE = struct.calcsize(ENDPOINT_HEADER_FMT)  # 2

MSG_UUID_SIZE = 16
TEXT_MSG_META_FMT = f"<H{MSG_UUID_SIZE}s"  # dst_id(u16 LE) uuid(16 bytes)
TEXT_MSG_META_SIZE = struct.calcsize(TEXT_MSG_META_FMT)  # 18

MAX_ENDPOINT_MSG_SIZE = 512
MAX_TEXT_PAYLOAD_SIZE = (
    MAX_ENDPOINT_MSG_SIZE - ENDPOINT_HEADER_SIZE - TEXT_MSG_META_SIZE
)  # 492


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------
@dataclass
class FrameHeader:
    """BLE service frame header (messaging_service_frame_header_t)."""

    version: int
    frame_type: int
    frag_total: int
    frag_idx: int
    seq_id: int

    def pack(self) -> bytes:
        return struct.pack(
            FRAME_HEADER_FMT,
            self.version,
            self.frame_type,
            self.frag_total,
            self.frag_idx,
            self.seq_id,
        )

    @classmethod
    def unpack(cls, data: bytes) -> "FrameHeader":
        return cls(*struct.unpack_from(FRAME_HEADER_FMT, data))


@dataclass
class TextMessage:
    """Application-level text message (messaging_endpoint_text_message_t)."""

    dst_id: int
    msg_uuid: bytes
    text: str

    def pack(self) -> bytes:
        header = struct.pack(ENDPOINT_HEADER_FMT, 0, MSG_TYPE_TEXT)
        meta = struct.pack(TEXT_MSG_META_FMT, self.dst_id, self.msg_uuid)
        return header + meta + self.text.encode("utf-8")

    @classmethod
    def unpack(cls, data: bytes) -> "TextMessage":
        _, msg_type = struct.unpack_from(ENDPOINT_HEADER_FMT, data)
        if msg_type != MSG_TYPE_TEXT:
            raise ValueError(f"Unknown message type: {msg_type:#x}")
        dst_id, msg_uuid = struct.unpack_from(
            TEXT_MSG_META_FMT, data, ENDPOINT_HEADER_SIZE
        )
        text_start = ENDPOINT_HEADER_SIZE + TEXT_MSG_META_SIZE
        text = data[text_start:].decode("utf-8", errors="replace").rstrip("\x00")
        return cls(dst_id, msg_uuid, text)

    @classmethod
    def create(cls, dst_id: int, text: str) -> "TextMessage":
        return cls(dst_id, _uuid.uuid4().bytes, text)
