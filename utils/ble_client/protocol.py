"""
Protocol definitions matching the firmware headers:
  - alfie_ble_service_protocol.h  (BLE service transport framing)
  - alfie_protocol.h              (Alfie protocol header)
  - alfie_messaging_proto.h       (messaging endpoint)
"""

import struct
import uuid as _uuid
from dataclasses import dataclass

# ---------------------------------------------------------------------------
# BLE Service UUIDs
# ---------------------------------------------------------------------------
ALFIE_BLE_SERVICE_UUID = "c1534fa3-5211-4e32-a176-d1af04513305"
ALFIE_BLE_SERVICE_DATA_CHAR_UUID = "c1534fa4-5211-4e32-a176-d1af04513305"

CONTROL_BLE_SERVICE_UUID = "7928884e-01e6-4137-86d3-adefd8afe21d"
CONTROL_BLE_SERVICE_DEVICE_ID_CHAR_UUID = "7928884f-01e6-4137-86d3-adefd8afe21d"

# ---------------------------------------------------------------------------
# BLE transport frame types (alfie_ble_service_proto_frame_type_t)
# ---------------------------------------------------------------------------
FRAME_TYPE_DATA = 0x00
FRAME_TYPE_DATA_ACK = 0x01

# ---------------------------------------------------------------------------
# BLE transport frame header (alfie_ble_service_proto_frame_header_t)
# Layout: version(u8) frame_type(u8)
# ---------------------------------------------------------------------------
FRAME_BASE_HEADER_FMT = "<BB"
FRAME_BASE_HEADER_SIZE = struct.calcsize(FRAME_BASE_HEADER_FMT)  # 2

# ---------------------------------------------------------------------------
# Data frame header (alfie_ble_service_proto_data_frame_t)
# Layout: base_header(2) + seq_id(u16) + total_size_bytes(u16) + frag_idx(u8) + frag_total(u8)
# ---------------------------------------------------------------------------
DATA_FRAME_HEADER_FMT = "<BBHHBB"
DATA_FRAME_HEADER_SIZE = struct.calcsize(DATA_FRAME_HEADER_FMT)  # 8

# ---------------------------------------------------------------------------
# ACK frame (alfie_ble_service_proto_data_ack_frame_t)
# Layout: base_header(2) + seq_id(u16) + frag_idx(u8)
# ---------------------------------------------------------------------------
ACK_FRAME_FMT = "<BBHB"
ACK_FRAME_SIZE = struct.calcsize(ACK_FRAME_FMT)  # 5

MAX_ATT_PAYLOAD = 251  # Assumes negotiated MTU of 254
MAX_DATA_FRAME_PAYLOAD = MAX_ATT_PAYLOAD - DATA_FRAME_HEADER_SIZE  # 243

# ---------------------------------------------------------------------------
# Alfie protocol header (alfie_proto_header_t)
# Layout: version(u8) endpoint_id(u8) src_id(u32) dst_id(u32)
# ---------------------------------------------------------------------------
ALFIE_HEADER_FMT = "<BBII"
ALFIE_HEADER_SIZE = struct.calcsize(ALFIE_HEADER_FMT)  # 10

ALFIE_MESSAGING_ENDPOINT_ID = 0x00

# ---------------------------------------------------------------------------
# Messaging endpoint (alfie_messaging_proto.h)
# ---------------------------------------------------------------------------
MESSAGING_FRAME_TYPE_TEXT = 0x00

# Messaging header: alfie_header(10) + frame_type(u8)
MESSAGING_HEADER_FMT = "<BBIIB"
MESSAGING_HEADER_SIZE = struct.calcsize(MESSAGING_HEADER_FMT)  # 11

MSG_UUID_SIZE = 16

MAX_ENDPOINT_MSG_SIZE = 512
MAX_TEXT_PAYLOAD_SIZE = MAX_ENDPOINT_MSG_SIZE - MESSAGING_HEADER_SIZE - MSG_UUID_SIZE  # 485


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------
@dataclass
class DataFrameHeader:
    """BLE transport data frame header (alfie_ble_service_proto_data_frame_t)."""

    version: int
    frame_type: int
    seq_id: int
    total_size_bytes: int
    frag_idx: int
    frag_total: int

    def pack(self) -> bytes:
        return struct.pack(
            DATA_FRAME_HEADER_FMT,
            self.version,
            self.frame_type,
            self.seq_id,
            self.total_size_bytes,
            self.frag_idx,
            self.frag_total,
        )

    @classmethod
    def unpack(cls, data: bytes) -> "DataFrameHeader":
        return cls(*struct.unpack_from(DATA_FRAME_HEADER_FMT, data))


@dataclass
class AckFrame:
    """BLE transport ACK frame (alfie_ble_service_proto_data_ack_frame_t)."""

    version: int
    frame_type: int
    seq_id: int
    frag_idx: int

    def pack(self) -> bytes:
        return struct.pack(
            ACK_FRAME_FMT,
            self.version,
            self.frame_type,
            self.seq_id,
            self.frag_idx,
        )

    @classmethod
    def unpack(cls, data: bytes) -> "AckFrame":
        return cls(*struct.unpack_from(ACK_FRAME_FMT, data))


@dataclass
class TextMessage:
    """Application-level text message (alfie_messaging_proto_text_frame_t)."""

    src_id: int
    dst_id: int
    msg_uuid: bytes
    text: str

    def pack(self) -> bytes:
        # Messaging header: alfie_header(version, endpoint_id, src_id, dst_id) + frame_type
        header = struct.pack(
            MESSAGING_HEADER_FMT,
            0,  # version
            ALFIE_MESSAGING_ENDPOINT_ID,
            self.src_id,
            self.dst_id,
            MESSAGING_FRAME_TYPE_TEXT,
        )
        return header + self.msg_uuid + self.text.encode("utf-8") + b"\x00"

    @classmethod
    def unpack(cls, data: bytes) -> "TextMessage":
        version, endpoint_id, src_id, dst_id, frame_type = struct.unpack_from(
            MESSAGING_HEADER_FMT, data
        )
        if frame_type != MESSAGING_FRAME_TYPE_TEXT:
            raise ValueError(f"Unknown messaging frame type: {frame_type:#x}")

        uuid_start = MESSAGING_HEADER_SIZE
        msg_uuid = data[uuid_start : uuid_start + MSG_UUID_SIZE]

        text_start = uuid_start + MSG_UUID_SIZE
        text = data[text_start:].decode("utf-8", errors="replace").rstrip("\x00")
        return cls(src_id, dst_id, msg_uuid, text)

    @classmethod
    def create(cls, src_id: int, dst_id: int, text: str) -> "TextMessage":
        return cls(src_id, dst_id, _uuid.uuid4().bytes, text)
