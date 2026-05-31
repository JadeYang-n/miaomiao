#!/usr/bin/env python
"""Simple MQTT broker using hbmqtt"""
import asyncio
from hbmqtt.broker import Broker

config = {
    'listeners': {
        'default': {
            'bind': '0.0.0.0:1883',
            'max_connections': 10,
        },
    },
    'allow_anonymous': True,
    'timeout': 10,
    'keepalive': 60,
}

async def start_broker():
    broker = Broker(config)
    await broker.start()
    print("MQTT Broker started on port 1883")
    try:
        await asyncio.Future()  # Run forever
    except KeyboardInterrupt:
        await broker.stop()

if __name__ == '__main__':
    asyncio.run(start_broker())