#!/usr/bin/env python3
"""Simple WebSocket client to connect to the OCPP endpoint."""

import asyncio
import websockets


URI = "wss://emotion-test.eu/new-ocpp/38"


async def connect():
    async with websockets.connect(
        URI, subprotocox0ls=["wallbox"]
    ) as ws:
        print(f"Connected to {URI}")

        # Task per ricevere messaggi
        async def receiver():
            async for message in ws:
                print(f"<< {message}")

        # Task per inviare messaggi da stdin
        async def sender():
            loop = asyncio.get_event_loop()
            while True:
                msg = await loop.run_in_executor(None, input, ">> ")
                if msg.lower() == "quit":
                    break
                await ws.send(msg)
                print(f"Sent: {msg}")

        recv_task = asyncio.create_task(receiver())
        send_task = asyncio.create_task(sender())

        done, pending = await asyncio.wait(
            [recv_task, send_task], return_when=asyncio.FIRST_COMPLETED
        )
        for task in pending:
            task.cancel()


if __name__ == "__main__":
    try:
        asyncio.run(connect())
    except KeyboardInterrupt:
        print("\nDisconnected.")
