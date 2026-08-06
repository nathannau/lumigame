# lumigame

A DIY arcade of 32 illuminated buttons, driven by an ESP32 — games are built visually with [Blockly](https://developers.google.com/blockly) so kids can design their own light games.

## Hardware

- **MCU**: ESP32 (DevKit V1, 38-pin)
- **Lights**: 32 individually addressable LEDs, wired as 4 strips of 8 (FastLED), data pins GPIO16, 17, 25, 26 (GPIO18/19/23/5, the VSPI pins, are kept free for a future SPI peripheral)
- **Buttons**: 32 inputs, multiplexed through two TCA9555 I²C GPIO expanders
- **I²C**: SDA = GPIO21, SCL = GPIO22

## Software

Built with [PlatformIO](https://platformio.org/) (Arduino framework) and [FastLED](https://fastled.io/).

Games are authored visually using [Maker Block Studio](https://marketplace.visualstudio.com/items?itemName=linucs.blocks-editor), a Blockly-based VS Code extension. Each game lives in its own `.blk`/`.cpp` pair under `src/`, while `main.cpp` owns the shared routing: initializing the hardware, driving the game-selection menu, and dispatching to the active game.

## Project status

Work in progress — hardware routing, the game menu, and the game-registration mechanism are still being built out.

## License

MIT — see [LICENSE](LICENSE).
