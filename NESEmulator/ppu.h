#pragma once
#include <cstdint>
#include <array>
#include <fstream>
#include <iostream>
#include <ctime>
#include "cartridge.h"

class PPU
{
private:
	// Registers
	uint8_t ppuctrl;
	uint8_t ppumask;
	uint8_t ppustatus;
	uint8_t oamaddr;
	uint32_t m_FrameCount;
	std::time_t m_SixtyFrameTimeStart;
	std::time_t m_SixtyFrameTimeEnd;

	// VRAM Access
	uint8_t latch;
	uint16_t vramAddr, tempAddr;
	uint8_t fineX;
	uint8_t dataBuffer;

	// Memory
	std::array<uint8_t, 256> oamData;
	std::array<uint8_t, 32> paletteRAM;
	std::array<uint8_t, 2048> nameTables;
	std::array<uint32_t, 256 * 240> frameBuffer;

	// Frame state
	int scanline;
	int cycle;

	// NMI Signaling
	bool nmiOccurred;
	bool nmiOutput;
	bool nmiPrevious;
	int nmiDelay;
	bool frameComplete;

	uint8_t dmaPage;
	bool dmaTriggered;

	Cartridge* cartridge;

	uint16_t bgPatternShiftLow;
	uint16_t bgPatternShiftHigh;
	uint16_t bgAttribShiftLow;
	uint16_t bgAttribShiftHigh;

	uint8_t nextTileID = 0;
	uint8_t nextTileAttrib = 0;
	uint8_t nextTileLSB = 0;
	uint8_t nextTileMSB = 0;

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

	// DEBUG
	int vramWrites;
	void clearDebugWrites() { vramWrites = 0; }
	void incrementDebugWrites() { vramWrites++; }

public:
	PPU(Cartridge* cart);

	void step(uint32_t cpuCycles);

	void writeRegister(uint16_t addr, uint8_t value);
	uint8_t readRegister(uint16_t addr);

	void writeVRAM(uint16_t addr, uint8_t value);
	uint8_t readVRAM(uint16_t addr);

	uint16_t mirrorNametableAddress(uint16_t addr);
	uint8_t mirrorPaletteAddress(uint16_t addr);

	bool isDMATriggered() { return (ppuctrl & 0x04) != 0; }
	void clearDMA() { ppuctrl &= ~0x04; }

	bool isNMITriggered() { return nmiDelay > 0; }
	void decNMIDelay() { if (nmiDelay > 0) nmiDelay--; }
	bool getNMI();
	void clearNMI() { nmiDelay = 0; }

	bool isFrameComplete() const { return frameComplete; }
	void resetFrameComplete() { frameComplete = false; }

	uint8_t getDMAPage() const { return dmaPage; }
	void setDMAPage(uint8_t page) { dmaPage = page; }

	int getScanline() const { return scanline; }
	int getCycle() const { return cycle; }

	void renderPixel();

	void incrementScrollX();
	void incrementScrollY();

	void copyHorizontalScrollBits();
	void copyVerticalScrollBits();

	void evaluateSprites();
	void fetchSpritePatterns();

    uint32_t* getFrameBuffer() const { return const_cast<uint32_t*>(frameBuffer.data()); }
	void dumpPatternTable(std::array<uint32_t, 128 * 128>& outBuffer, int tableIndex);
	void dumpNametable();
};

//static const uint32_t NESPalette[64] = {
//	0x626262FF, 0x011c94FF, 0x1905adFF, 0x42009dFF, // $00–$03
//	0x60016bFF, 0x6f0125FF, 0x650401FF, 0x491e00FF, // $04–$07
//	0x223601FF, 0x014901FF, 0x004f00FF, 0x004816FF, // $08–$0B
//	0x01355fFF, 0x000000FF, 0x000000FF, 0x000000FF, // $0C–$0F
//	0xabababFF, 0x0c4edbFF, 0x3d2ffeFF, 0x7014f3FF, // $10–$13
//	0x9b0bb9FF, 0xb11263FF, 0xa82705FF, 0x884601FF, // $14–$17
//	0x566601FF, 0x227f01FF, 0x008801FF, 0x008233FF, // $18–$1B
//	0x006c91FF, 0x000000FF, 0x000000FF, 0x000000FF, // $1C–$1F
//	0xfefffeFF, 0x57a4feFF, 0x8286feFF, 0xb46cfeFF, // $20–$23
//	0xb46cfeFF, 0xf962c6FF, 0xf8746dFF, 0xde9121FF, // $24–$27
//	0xb2ae01FF, 0x81c901FF, 0x56d423FF, 0x3cd26fFF, // $28–$2B
//	0x3ec1c8FF, 0x4e4f4fFF, 0x000000FF, 0x000000FF, // $2C–$2F
//	0xfefffeFF, 0xbee0ffFF, 0xcdd4ffFF, 0xe0cbfeFF, // $30–$33
//	0xf1c4ffFF, 0xfdc5efFF, 0xfccacfFF, 0xf5d5aeFF, // $34–$37 
//	0xe6df9cFF, 0xd2e89aFF, 0xc2eea9FF, 0xc2eea9FF, // $38–$3B
//	0xb6eae5FF, 0xb9b8b9FF, 0x000000FF, 0x000000FF, // $3C–$3F
//};

static const uint32_t NESPalette[64] = {
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

