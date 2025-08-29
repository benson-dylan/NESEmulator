#pragma once
#include <cstdint>
#include <array>
#include <fstream>
#include <iostream>
#include <ctime>
#include "cartridge.h"

class NEW_PPU
{
	private:
		// Registers
		uint8_t PPUCTRL;
		uint8_t PPUMASK;
		uint8_t PPUSTATUS;
		uint8_t OAMADDR;
		uint16_t PPUSCROLL;
		uint16_t PPUADDR;
		uint8_t PPUDATA;

		// Internal Registers
		uint16_t v, t; // VRAM and temp
		uint8_t x, w; // Fine X and Latch

		// Scanlines, Dots, Frames
		int scanline, cycle, frame;
		bool frameComplete;

		Cartridge* cartridge;

		// Arrays
		std::array<uint8_t, 256> oamData;
		std::array<uint8_t, 32> paletteRAM;
		std::array<uint8_t, 2048> nameTables;
		std::array<uint32_t, 256 * 240> frameBuffer;

		// Tile Info
		uint8_t tileID, attrByte;
		uint8_t tileLSB, tileMSB;
		uint8_t buffer;

		uint16_t bgPatternShiftLow;
		uint16_t bgPatternShiftHigh;
		uint16_t bgAttribShiftLow;
		uint16_t bgAttribShiftHigh;

		uint8_t tileAttrib;

		// Sprite Info
		struct Sprite
		{
			uint8_t y;           // Y position
			uint8_t x;           // X position
			uint8_t tileID;      // Tile ID
			uint8_t attributes;  // Attributes (palette, flip, etc.)
			uint8_t patternLow;  // Low byte of the sprite pattern
			uint8_t patternHigh; // High byte of the sprite pattern
		};

		Sprite spriteScanline[8];
		uint8_t spriteCount;
		bool spriteZeroHit = false;

		uint8_t dmaPage;
		int nmiDelay;
		bool nmiOccurred;

		bool startDMA;


	public:
		NEW_PPU(Cartridge* cart);

		void step(uint32_t cpuCycles);

		void copyVerticalScrollBits();
		void copyHorizontalScrollBits();

		void incrementX();
		void incrementY();

		// Read/Write to PPU Registers
		uint8_t readRegister(uint16_t addr);
		void writeRegister(uint16_t addr, uint8_t value);

		// Read/Write to VRAM Address
		uint8_t readVRAM(uint16_t addr);
		void writeVRAM(uint16_t addr, uint8_t value);

		uint16_t mirrorNametableAddress(uint16_t addr);
		uint8_t mirrorPaletteAddress(uint16_t addr);

		uint8_t getDMAPage() const { return dmaPage; }
		void setDMAPage(uint8_t page) { dmaPage = page; startDMA = true; }
		bool isDMATriggered() { return startDMA; }
		void clearDMA() { startDMA = false; }

		void clearNMI() { nmiDelay = 0; }
		bool getNMI();

		bool isFrameComplete() const { return frameComplete; }
		void resetFrameComplete() { frameComplete = false; }

		void evaluateSprites();
		void fetchSpritePatterns();

		void renderPixel();

		uint32_t* getFrameBuffer() const { return const_cast<uint32_t*>(frameBuffer.data()); }

		// DEBUG
		void dumpNametable();
		int getScanline() const { return scanline; }
		int getCycle() const { return cycle; }
};

static const uint32_t Palette[64] = {
	0x545454FF, 0x001E74FF, 0x081090FF, 0x300088FF,
	0x440064FF, 0x5C0030FF, 0x540400FF, 0x3C1800FF,
	0x202A00FF, 0x083A00FF, 0x003C00FF, 0x003A10FF,
	0x002840FF, 0x000000FF, 0x000000FF, 0x000000FF,
	0x989898FF, 0x084CC4FF, 0x3032ECFF, 0x5C1EE4FF,
	0x8814B0FF, 0xA01464FF, 0x982220FF, 0x783C00FF,
	0x545A00FF, 0x287200FF, 0x087C00FF, 0x007628FF,
	0x006678FF, 0x000000FF, 0x000000FF, 0x000000FF,
	0xECECECFF, 0x4C9AEFFF, 0x787CFFFF, 0xB062FFFF,
	0xE454E4FF, 0xFC58B8FF, 0xF87858FF, 0xFCA044FF,
	0xF0C000FF, 0xA0D800FF, 0x48E400FF, 0x00CC44FF,
	0x00B4CCFF, 0x3C3C3CFF, 0x000000FF, 0x000000FF,
	0xECECECFF, 0xA8D8F8FF, 0xB8B8FFFF, 0xD8B8F8FF,
	0xF8B8F8FF, 0xF8B8D8FF, 0xF8C8B8FF, 0xF0D8A8FF,
	0xF0E4A0FF, 0xC8F0A0FF, 0xA8F0B8FF, 0xB8F8B8FF,
	0xB8F8D8FF, 0x787878FF, 0x000000FF, 0x000000FF
};

