import argparse
import queue
import struct
import time

import numpy as np
import serial
import sounddevice as sd

SAMPLE_RATE = 8000
BLOCK_SIZE = 160
SYNC = b"\xAA\x55"

audio_queue = queue.Queue(maxsize=40)

def audio_callback(indata, frames, time_info, status):
    if status:
        print("Audio status:", status)

    try:
        audio_queue.put_nowait(indata[:, 0].copy())
    except queue.Full:
        pass

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--device", type=int, default=None)
    args = parser.parse_args()

    print("Opening serial port:", args.port)

    with serial.Serial(args.port, 2000000, timeout=0.2) as esp:
        time.sleep(2.0)
        esp.reset_input_buffer()

        print("Using microphone device:", args.device)
        print("Streaming PCM16 microphone audio at 8 kHz.")
        print("Speak or play a 1 kHz tone near the microphone.")
        print("Press Ctrl+C to stop.")

        with sd.InputStream(
            samplerate=SAMPLE_RATE,
            blocksize=BLOCK_SIZE,
            channels=1,
            dtype="float32",
            device=args.device,
            callback=audio_callback,
        ):
            while True:
                samples_float = audio_queue.get()

                # Limit amplitude to prevent DAC clipping.
                samples_float = np.clip(samples_float * 1.00, -1.0, 1.0)

                samples_i16 = (samples_float * 32767).astype("<i2")
                payload = samples_i16.tobytes()

                packet = SYNC + struct.pack("<H", len(samples_i16)) + payload
                esp.write(packet)

if __name__ == "__main__":
    main()