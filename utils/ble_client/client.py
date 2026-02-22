"""
BLE messaging client with fragmentation and reassembly.

Implements the same framing protocol as messaging_service.c on the firmware
side: payload frames are fragmented and each fragment is ACK'd before the
next is sent.  Incoming notifications are reassembled the same way.
"""

import asyncio
import logging
import math
import random
from dataclasses import dataclass, field
from typing import Callable, Optional

from bleak import BleakClient, BleakScanner
from bleak.backends.device import BLEDevice

from protocol import (
    MESSAGING_SERVICE_UUID,
    MESSAGING_SERVICE_DATA_CHAR_UUID,
    FRAME_HEADER_SIZE,
    MAX_FRAME_PAYLOAD,
    FRAME_TYPE_ACK,
    FRAME_TYPE_PAYLOAD,
    FrameHeader,
    TextMessage,
)

logger = logging.getLogger(__name__)

ACK_TIMEOUT_S = 2.5
MAX_TX_ATTEMPTS = 4


@dataclass
class _ReassemblyCtx:
    seq_id: int
    frag_total: int
    frag_idx: int = 0
    buffer: bytearray = field(default_factory=bytearray)


class MessagingClient:
    def __init__(self):
        self._client: Optional[BleakClient] = None
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._connected = False
        self._rx_contexts: dict[int, _ReassemblyCtx] = {}
        self._ack_event = asyncio.Event()
        self._on_message: Optional[Callable[[TextMessage], None]] = None
        self._on_connection_change: Optional[Callable[[bool], None]] = None
        self._on_error: Optional[Callable[[str], None]] = None
        self._scan_results: list[BLEDevice] = []
        self._max_frame_payload = MAX_FRAME_PAYLOAD

    @property
    def connected(self) -> bool:
        return (
            self._connected
            and self._client is not None
            and self._client.is_connected
        )

    def set_callbacks(
        self,
        on_message: Optional[Callable[[TextMessage], None]] = None,
        on_connection_change: Optional[Callable[[bool], None]] = None,
        on_error: Optional[Callable[[str], None]] = None,
    ):
        self._on_message = on_message
        self._on_connection_change = on_connection_change
        self._on_error = on_error

    async def scan(self, timeout: float = 5.0) -> list[BLEDevice]:
        devices = await BleakScanner.discover(timeout=timeout)
        self._scan_results = list(devices)
        return self._scan_results

    async def connect(self, address: str):
        self._loop = asyncio.get_running_loop()
        self._client = BleakClient(
            address, disconnected_callback=self._on_disconnected
        )
        await self._client.connect()

        # Adapt fragment size to the negotiated MTU
        mtu = self._client.mtu_size
        if mtu and mtu > 0:
            self._max_frame_payload = min(
                MAX_FRAME_PAYLOAD, mtu - 3 - FRAME_HEADER_SIZE
            )

        await self._client.start_notify(
            MESSAGING_SERVICE_DATA_CHAR_UUID, self._on_notification
        )
        self._connected = True

    async def disconnect(self):
        if self._client and self._client.is_connected:
            await self._client.disconnect()
        self._connected = False

    # ------------------------------------------------------------------
    # Internal: BLE callbacks
    # ------------------------------------------------------------------
    def _on_disconnected(self, _client: BleakClient):
        self._connected = False
        self._rx_contexts.clear()
        if self._on_connection_change:
            self._on_connection_change(False)

    def _on_notification(self, _sender, data: bytearray):
        if len(data) < FRAME_HEADER_SIZE:
            return
        header = FrameHeader.unpack(bytes(data))

        if header.frame_type == FRAME_TYPE_ACK:
            self._ack_event.set()
            return

        if header.frame_type == FRAME_TYPE_PAYLOAD:
            self._handle_rx_payload(header, bytes(data[FRAME_HEADER_SIZE:]))

    # ------------------------------------------------------------------
    # Internal: RX reassembly
    # ------------------------------------------------------------------
    def _handle_rx_payload(self, header: FrameHeader, payload: bytes):
        ctx = self._rx_contexts.get(header.seq_id)

        if ctx is None:
            if header.frag_idx != 0:
                return
            ctx = _ReassemblyCtx(
                seq_id=header.seq_id, frag_total=header.frag_total
            )
            self._rx_contexts[header.seq_id] = ctx

        # Duplicate fragment — re-ACK
        if header.frag_idx < ctx.frag_idx:
            if self._loop:
                self._loop.create_task(self._send_ack(header))
            return

        # Out-of-order fragment — ignore
        if header.frag_idx != ctx.frag_idx:
            return

        ctx.buffer.extend(payload)
        ctx.frag_idx += 1

        if self._loop:
            self._loop.create_task(self._send_ack(header))

        if ctx.frag_idx == ctx.frag_total:
            del self._rx_contexts[header.seq_id]
            self._deliver_message(bytes(ctx.buffer))

    async def _send_ack(self, header: FrameHeader):
        if not self.connected:
            return
        ack = FrameHeader(
            version=0,
            frame_type=FRAME_TYPE_ACK,
            frag_total=header.frag_total,
            frag_idx=header.frag_idx,
            seq_id=header.seq_id,
        )
        try:
            await self._client.write_gatt_char(
                MESSAGING_SERVICE_DATA_CHAR_UUID, ack.pack(), response=True
            )
        except Exception:
            logger.warning("Failed to send ACK", exc_info=True)

    def _deliver_message(self, data: bytes):
        if not self._on_message:
            return
        try:
            msg = TextMessage.unpack(data)
            self._on_message(msg)
        except Exception as e:
            logger.error("Failed to parse incoming message", exc_info=True)
            if self._on_error:
                self._on_error(f"Failed to parse message ({len(data)} bytes): {e}")

    # ------------------------------------------------------------------
    # Public: send
    # ------------------------------------------------------------------
    async def send_text(self, dst_id: int, text: str) -> TextMessage:
        if not self.connected:
            raise ConnectionError("Not connected")
        msg = TextMessage.create(dst_id, text)
        await self._send_fragmented(msg.pack())
        return msg

    async def _send_fragmented(self, data: bytes):
        seq_id = random.randint(0, 0xFFFF)
        frag_total = max(1, math.ceil(len(data) / self._max_frame_payload))

        for i in range(frag_total):
            offset = i * self._max_frame_payload
            chunk = data[offset : offset + self._max_frame_payload]

            header = FrameHeader(
                version=0,
                frame_type=FRAME_TYPE_PAYLOAD,
                frag_total=frag_total,
                frag_idx=i,
                seq_id=seq_id,
            )
            frame = header.pack() + chunk

            for attempt in range(MAX_TX_ATTEMPTS):
                self._ack_event.clear()
                await self._client.write_gatt_char(
                    MESSAGING_SERVICE_DATA_CHAR_UUID, frame, response=True
                )
                try:
                    await asyncio.wait_for(
                        self._ack_event.wait(), ACK_TIMEOUT_S
                    )
                    break
                except asyncio.TimeoutError:
                    logger.warning(
                        "ACK timeout frag %d/%d attempt %d",
                        i,
                        frag_total,
                        attempt + 1,
                    )
            else:
                raise TimeoutError(
                    f"No ACK for fragment {i} after {MAX_TX_ATTEMPTS} attempts"
                )
