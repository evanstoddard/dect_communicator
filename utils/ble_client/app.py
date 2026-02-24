"""
DECT NR+ Communicator — Textual TUI for BLE messaging via Alfie protocol.

Usage:
    cd utils/ble_client
    pip install -r requirements.txt
    python app.py

Commands (type in the input bar):
    /scan              Scan for nearby devices advertising the Alfie BLE service
    /connect <#|addr>  Connect by scan index or BLE address
    /disconnect        Disconnect from device
    /src <id>          Set the source device ID (decimal)
    /dst <id>          Set the destination device ID (decimal)
    /help              Show available commands
"""

from datetime import datetime

from rich.markup import escape

from textual.app import App, ComposeResult
from textual.widgets import Header, Footer, RichLog, Input, Static
from textual import work

from client import MessagingClient
from protocol import TextMessage, MAX_TEXT_PAYLOAD_SIZE


class StatusBar(Static):
    """Single-line bar showing connection state, source ID, and destination ID."""


class MessagingApp(App):
    TITLE = "DECT NR+ Communicator"

    CSS = """
    StatusBar {
        dock: top;
        height: 1;
        background: $surface;
        color: $text-muted;
        padding: 0 1;
    }

    #message-log {
        height: 1fr;
        border: solid $accent;
        padding: 0 1;
    }

    Input {
        dock: bottom;
    }
    """

    BINDINGS = [
        ("ctrl+q", "quit", "Quit"),
    ]

    def __init__(self):
        super().__init__()
        self.client = MessagingClient()
        self.client.set_callbacks(
            on_message=self._on_ble_message,
            on_connection_change=self._on_ble_connection_change,
            on_error=self._on_ble_error,
        )
        self.src_id = 0
        self.dst_id = 1

    def compose(self) -> ComposeResult:
        yield Header()
        yield StatusBar(self._status_text())
        yield RichLog(id="message-log", highlight=True, markup=True)
        yield Input(placeholder="Type a message or /help for commands")
        yield Footer()

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------
    def _status_text(self) -> str:
        conn = (
            "[green]Connected[/green]"
            if self.client.connected
            else "[red]Disconnected[/red]"
        )
        return f"{conn}  |  SRC ID: {self.src_id:#010x}  |  DST ID: {self.dst_id:#010x}"

    def _update_status(self):
        self.query_one(StatusBar).update(self._status_text())

    def _log(self, text: str):
        ts = datetime.now().strftime("%H:%M:%S")
        self.query_one("#message-log", RichLog).write(f"[dim]{ts}[/dim] {text}")

    def _safe_call(self, fn, *args):
        """Call fn from any thread — works from both the app thread and workers."""
        try:
            self.call_from_thread(fn, *args)
        except RuntimeError:
            fn(*args)

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------
    def on_mount(self):
        self._log("[bold]Welcome to DECT NR+ Communicator[/bold]")
        self._log("Type [bold]/help[/bold] for available commands.")

    # ------------------------------------------------------------------
    # Input handling
    # ------------------------------------------------------------------
    def on_input_submitted(self, event: Input.Submitted) -> None:
        text = event.value.strip()
        event.input.clear()
        if not text:
            return
        if text.startswith("/"):
            self._handle_command(text)
        else:
            self._do_send(text)

    def _handle_command(self, cmd: str):
        parts = cmd.split(maxsplit=1)
        command = parts[0].lower()
        args = parts[1].strip() if len(parts) > 1 else ""

        if command == "/help":
            self._log("[bold]Commands:[/bold]")
            self._log("  [bold]/scan[/bold]              Scan for devices")
            self._log(
                "  [bold]/connect[/bold] <#|addr>  "
                "Connect (scan index or BLE address)"
            )
            self._log("  [bold]/disconnect[/bold]         Disconnect from device")
            self._log("  [bold]/src[/bold] <id>           Set source device ID")
            self._log("  [bold]/dst[/bold] <id>           Set destination device ID")
            self._log("  [bold]/help[/bold]              Show this help")
        elif command == "/scan":
            self._do_scan()
        elif command == "/connect":
            if not args:
                self._log("[red]Usage: /connect <index or address>[/red]")
                return
            self._do_connect(args)
        elif command == "/disconnect":
            self._do_disconnect()
        elif command == "/src":
            try:
                self.src_id = int(args, 0)
                self._update_status()
                self._log(f"Source ID set to [bold]{self.src_id:#010x}[/bold]")
            except ValueError:
                self._log("[red]Usage: /src <number>[/red]")
        elif command == "/dst":
            try:
                self.dst_id = int(args, 0)
                self._update_status()
                self._log(f"Destination ID set to [bold]{self.dst_id:#010x}[/bold]")
            except ValueError:
                self._log("[red]Usage: /dst <number>[/red]")
        else:
            self._log(f"[red]Unknown command: {escape(command)}[/red]")

    # ------------------------------------------------------------------
    # BLE workers
    # ------------------------------------------------------------------
    @work(exclusive=True, group="scan")
    async def _do_scan(self):
        self._log("[yellow]Scanning...[/yellow]")
        try:
            devices = await self.client.scan(timeout=5.0)
        except Exception as e:
            self._log(f"[red]Scan failed: {escape(str(e))}[/red]")
            return
        if not devices:
            self._log("[yellow]No devices found.[/yellow]")
            return
        self._log(f"[green]Found {len(devices)} device(s):[/green]")
        for i, dev in enumerate(devices):
            name = dev.name or "Unknown"
            self._log(f"  [bold]{i}[/bold]: {escape(name)} ({dev.address})")

    @work(exclusive=True, group="connect")
    async def _do_connect(self, target: str):
        try:
            idx = int(target)
            if 0 <= idx < len(self.client._scan_results):
                dev = self.client._scan_results[idx]
                address = dev.address
                label = dev.name or address
            else:
                self._log(f"[red]Invalid index: {idx}[/red]")
                return
        except ValueError:
            address = target
            label = target

        self._log(f"Connecting to [bold]{escape(label)}[/bold]...")
        try:
            await self.client.connect(address)
            self._log("[green]Connected![/green]")
            if self.client.device_id is not None:
                self.src_id = self.client.device_id
                self._log(
                    f"Device ID: [bold]{self.src_id:#010x}[/bold] (set as SRC ID)"
                )
            self._update_status()
        except Exception as e:
            self._log(f"[red]Connection failed: {escape(str(e))}[/red]")

    @work(exclusive=True, group="disconnect")
    async def _do_disconnect(self):
        try:
            await self.client.disconnect()
            self._log("[yellow]Disconnected.[/yellow]")
            self._update_status()
        except Exception as e:
            self._log(f"[red]Disconnect error: {escape(str(e))}[/red]")

    @work(exclusive=True, group="send")
    async def _do_send(self, text: str):
        if not self.client.connected:
            self._log("[red]Not connected. Use /scan and /connect first.[/red]")
            return

        encoded_len = len(text.encode("utf-8"))
        if encoded_len > MAX_TEXT_PAYLOAD_SIZE:
            self._log(
                f"[red]Message too long "
                f"({encoded_len} bytes, max {MAX_TEXT_PAYLOAD_SIZE}).[/red]"
            )
            return

        self._log(f"[cyan]TX →[/cyan] {escape(text)}")
        try:
            await self.client.send_text(self.src_id, self.dst_id, text)
        except Exception as e:
            self._log(f"[red]Send failed: {escape(str(e))}[/red]")

    # ------------------------------------------------------------------
    # BLE callbacks (may fire from any thread)
    # ------------------------------------------------------------------
    def _on_ble_message(self, msg: TextMessage):
        uuid_short = msg.msg_uuid.hex()[:8]
        self._safe_call(
            self._log,
            f"[magenta]RX ←[/magenta] {escape(msg.text)}  "
            f"[dim](src:{msg.src_id:#010x} dst:{msg.dst_id:#010x} id:{uuid_short})[/dim]",
        )

    def _on_ble_error(self, error: str):
        self._safe_call(
            self._log, f"[red]BLE error: {escape(error)}[/red]"
        )

    def _on_ble_connection_change(self, connected: bool):
        if connected:
            self._safe_call(
                self._log, "[green]Device connected.[/green]"
            )
        else:
            self._safe_call(
                self._log, "[yellow]Device disconnected.[/yellow]"
            )
        self._safe_call(self._update_status)


def main():
    app = MessagingApp()
    app.run()


if __name__ == "__main__":
    main()
