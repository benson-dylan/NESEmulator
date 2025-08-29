#include "new_ppu.h"
#include "ppu.h"
#include <iomanip>

NEW_PPU::NEW_PPU(Cartridge* cart)
{
	PPUCTRL = 0x00;
	PPUMASK = 0x00;
	PPUSTATUS = 0xA0;
	OAMADDR = 0x00;
	PPUSCROLL = 0x0000;
	PPUADDR = 0x0000;
	PPUDATA = 0x00;

	v = t = 0x0000;
	x = w = 0x00;

	scanline = cycle = frame = 0;
	frameComplete = false;

	cartridge = cart;

	// Initialize Array Data
	std::fill(std::begin(oamData), std::end(oamData), 0);
	std::fill(std::begin(paletteRAM), std::end(paletteRAM), 0);
	std::fill(std::begin(nameTables), std::end(nameTables), 0x00);
	std::fill(std::begin(frameBuffer), std::end(frameBuffer), 0);

	tileID = 0x00;
	buffer = 0x00;

	startDMA = false;
}

void NEW_PPU::step(uint32_t cpuCycles)
{
	//printf("Frame: %d, Scanline: %d, Cycle: %d\n", frame, scanline, cycle);

	if (frame % 60 == 0)
	{
		//printf("Frame: %d\n", frame);
	}

	if (frame == 600)
	{
		dumpNametable();
	}

	for (int i = 0; i < cpuCycles * 3; i++)
	{ 
		PPUSTATUS |= 0x40;
		if (scanline == 261)
		{
			// Pre-render
			bool skip = frame % 2 == 1;
			if (cycle >= 280 && cycle <= 304 && PPUMASK & 0x18)
			{
				copyVerticalScrollBits();
			}
			PPUSTATUS &= ~0x80;
		}
		else if (scanline >= 0 && scanline <= 239 && PPUMASK & 0x18)
		{
			// Idle Cycle
			if (cycle == 0)
			{
				// Nuffin
			}
			// Visible Scanlines
			else if (cycle >= 1 && cycle <= 256)
			{
				if ((PPUMASK & 0x08) || (PPUMASK & 0x10))
				{
					bgPatternShiftLow <<= 1;
					bgPatternShiftHigh <<= 1;
					bgAttribShiftLow <<= 1;
					bgAttribShiftHigh <<= 1;
				}

				// Background Evaluation
				switch (cycle % 8)
				{
					case 0:
						bgPatternShiftLow = (bgPatternShiftLow) | tileLSB;
						bgPatternShiftHigh = (bgPatternShiftHigh) | tileMSB;
						
						bgAttribShiftLow = (bgAttribShiftLow) | ((attrByte & 1) ? 0xFF : 0x00);
						bgAttribShiftHigh = (bgAttribShiftHigh) | ((attrByte & 2) ? 0xFF : 0x00);
						break;
					case 1:
						// Nametable Byte
						// Fetch tile ID from nametable
						tileID = readVRAM(0x2000 | (v & 0x0FFF));
						break;
					case 3:
					{
						// Attribute Byte
						attrByte = readVRAM(0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07));
						incrementX();
						break;
					}
					case 5:
						// Pattern Low
						tileLSB = readVRAM(((PPUCTRL & 0x10) ? 0x1000 : 0x0000) + tileID * 16 + ((v >> 12) & 0x07));
						break;
					case 7:
						// Pattern High
						tileMSB = readVRAM(((PPUCTRL & 0x10) ? 0x1000 : 0x0000) + tileID * 16 + ((v >> 12) & 0x07) + 8);
						break;
				}

				if (cycle == 65)
				{
					//Sprite Evaluation
					evaluateSprites();
					fetchSpritePatterns();
				}

				if (cycle == 256)
					incrementY();

				renderPixel();
			}
			else if (cycle >= 257 && cycle <= 320)
			{
				// Tile Data for Next Scanline
				if (cycle == 257)
					copyHorizontalScrollBits();

				switch (cycle % 8)
				{
					case 1:
						// Nametable Byte
						// Fetch tile ID from nametable
						tileID = readVRAM(0x2000 | (v & 0x0FFF));
						break;
					case 3:
						// Attribute Byte
						attrByte = readVRAM(0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07));
						break;
					case 5:
						// Pattern Low
						tileLSB = readVRAM(((PPUCTRL & 0x10) ? 0x1000 : 0x0000) + tileID * 16 + ((v >> 12) & 0x07));
						break;
					case 7:
						// Pattern High
						tileMSB = readVRAM(((PPUCTRL & 0x10) ? 0x1000 : 0x0000) + tileID * 16 + ((v >> 12) & 0x07) + 8);
						break;
				}
			}
			else if (cycle >= 321 && cycle <= 336)
			{
				switch (cycle % 8)
				{
					case 1:
						// Nametable Byte
						// Fetch tile ID from nametable
						tileID = readVRAM(0x2000 | (v & 0x0FFF));
						break;
					case 3:
						// Attribute Byte
						attrByte = readVRAM(0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07));
						incrementX();
						break;
					case 5:
						// Pattern Low
						tileLSB = readVRAM(((PPUCTRL & 0x10) ? 0x1000 : 0x0000) + tileID * 16 + ((v >> 12) & 0x07));
						break;
					case 7:
						// Pattern High
						tileMSB = readVRAM(((PPUCTRL & 0x10) ? 0x1000 : 0x0000) + tileID * 16 + ((v >> 12) & 0x07) + 8);
						break;
				}
			}
			else if (cycle >= 337 && cycle <= 340)
			{
				// Two Bytes Fetched, Unknown Purpose
				switch (cycle % 2)
				{
					case 1:
						tileID = readVRAM(0x2000 | (v & 0x0FFF));
						break;
				}
			}
		}
		else if (scanline == 240)
		{
			// Post-render Scanline
		}
		else if (scanline >= 241 && scanline <= 260)
		{
			// Vblank Lines
			if (scanline == 241 && cycle == 1)
			{
				//printf("VBLANK TIME\n");
				PPUSTATUS |= 0x80;
				if (PPUCTRL & 0x80)
				{
					nmiOccurred = true;
					nmiDelay = 2;
				}
			}
		}
		else if (scanline == 262)
		{
			// Frame Complete
			scanline = 0;
			frame++;
			frameComplete = true;
			PPUSTATUS &= ~0x80; // VBlank Clear
			PPUSTATUS &= ~0x40; // Sprite Overflow Clear
			PPUSTATUS &= ~0x20; // Sprite 0 Hit Clear
			nmiOccurred = false;
		}

		if (cycle > 340)
		{
			cycle = 0;
			scanline++;
		}

		cycle++;
	}
}

void NEW_PPU::copyVerticalScrollBits()
{
	// Clears vertical bits and copies from temp
	v = (v & 0x041F) | (t & 0x7BE0);
}

void NEW_PPU::copyHorizontalScrollBits()
{
	// Clears horizontal bits and copies from temp
	v = (v & 0x7BE0) | (t & 0x041F);
}

void NEW_PPU::incrementX()
{
	if ((v & 0x001F) == 31)
	{
		v &= ~0x001F;
		v ^= 0x0400;
	}
	else
		v += 1;
}

void NEW_PPU::incrementY()
{
	if ((v & 0x7000) != 0x7000)
	{
		v += 0x1000;
	}
	else
	{
		v &= ~0x7000;
		int y = (v & 0x03E0) >> 5;
		if (y == 29)
		{
			y = 0;
			v ^= 0x0800;
		}
		else if (y == 31)
			y = 0;
		else
			y += 1;
		v = (v & ~0x03E0) | (y << 5);
	}
}

uint8_t NEW_PPU::readRegister(uint16_t addr)
{
	uint8_t result = 0;
	addr += 0x2000;

	switch (addr)
	{
		case 0x2002:
			result = PPUSTATUS & 0xE0;
			PPUSTATUS &= ~0x80;
			w = 0;
			break;
		case 0x2004:
			result = oamData[OAMADDR];
			break;
		case 0x2007:
			if (v >= 0x3F00)
				result = readVRAM(v);
			else
			{
				result = buffer;
				buffer = readVRAM(v);
			}

			v += (PPUCTRL & 0x04) ? 32 : 1;
			break;
	}

	return result;
}

void NEW_PPU::writeRegister(uint16_t addr, uint8_t value)
{
	addr = addr + 0x2000;

	switch (addr)
	{
		case 0x2000:
			PPUCTRL = value;
			//PPUCTRL |= 0x80;
			t = (t & 0xF3FF) | ((value & 0x03) << 10);
			break;
		case 0x2001:
			PPUMASK = value;
			break;
		case 0x2003:
			OAMADDR = value;
			break;
		case 0x2004:
			//printf("OAM Write: %02X to address %02X\n", value, OAMADDR);
			oamData[OAMADDR++] = value;
			break;
		case 0x2005:
			if (w == 0)
			{
				t = (t & 0xFFE0) | (value >> 3);
				x = value & 0x07;
				w = 1;
			}
			else
			{
				t = (t & 0x8fff) | ((value & 0x07) << 12);
				t = (t & 0xFC1F) | ((value & 0xF8) << 2);
				w = 0;
			}
			break;
		case 0x2006:
			if (w == 0)
			{
				t = (t & 0x60FF) | ((value & 0x3F) << 8);
				t &= 0xBFFF;
				w = 1;
			}
			else
			{
				t = (t & 0xFF00) | (value & 0xFF);
				v = t;
				w = 0;
			}
			break;
		case 0x2007:
			writeVRAM(v, value);
			v += (PPUCTRL & 0x04) ? 32 : 1; // Increment Mode
			break;
	}
}

uint8_t NEW_PPU::readVRAM(uint16_t addr)
{
	addr &= 0x3FFF;

	if (addr < 0x2000)
	{
		return cartridge->chrRead(addr);
	}
	else if (addr < 0x3000)
	{
		return nameTables[mirrorNametableAddress(addr)];
	}
	else if (addr < 0x4000)
	{
		return paletteRAM[mirrorPaletteAddress(addr)];
	}
}

void NEW_PPU::writeVRAM(uint16_t addr, uint8_t value)
{
	addr &= 0x3FFF;

	if (addr < 0x2000)
	{
		cartridge->chrWrite(addr, value);
	}
	if (addr < 0x3000)
	{
		//printf("Attempting to write to NameTable at address %04X with value %02X\n", mirrorNametableAddress(addr), value);
		nameTables[mirrorNametableAddress(addr)] = value;
	}
	else if (addr < 0x4000)
	{
		paletteRAM[mirrorPaletteAddress(addr)] = value;
	}
}

uint16_t NEW_PPU::mirrorNametableAddress(uint16_t addr)
{
	// Grab last 3 digits
	addr &= 0x0FFF;

	// Determine which table it is trying to access
	// Table 0 - 0x000 Table 1 - 0x400
	// Table 2 - 0x800 Table 3 - 0xC00
	uint16_t table = addr / 0x400;
	uint16_t offset = addr % 0x400;

	switch (cartridge->getMode())
	{
		case Cartridge::Vertical:
			//printf("Vertical Mirroring\n");
			if (table == 2) table = 0;
			if (table == 3) table = 1;
			break;
		case Cartridge::Horizontal:
			if (table == 1) table = 0;
			if (table == 3 || table == 2) table = 1;
			break;
		case Cartridge::SingleScreenLower:
			table = 0;
			break;
		case Cartridge::SingleScreenUpper:
			table = 1;
			break;
		case Cartridge::FourScreen:
			break;
	}

	return table * 0x400 + offset;
}

uint8_t NEW_PPU::mirrorPaletteAddress(uint16_t addr)
{
	addr &= 0x1F;
	if (addr == 0x10) return 0x00;
	if (addr == 0x14) return 0x04;
	if (addr == 0x18) return 0x08;
	if (addr == 0x1C) return 0x0C;
	return addr;
}

bool NEW_PPU::getNMI()
{
	if (nmiDelay > 0)
	{
		nmiDelay--;
		if (nmiDelay == 0 && nmiOccurred && (PPUCTRL & 0x80))
		{
			//printf("PPU triggered NMI at scanline %d, cycle %d\n", scanline, cycle);
			return true; // NMI triggered
		}
	}
	return false; // No NMI triggered
}

void NEW_PPU::evaluateSprites()
{
	spriteCount = 0;
	spriteZeroHit = false;
	uint8_t spriteHeight = (PPUCTRL & 0x20) ? 16 : 8;

	for (int i = 0; i < 64; ++i)
	{
		uint8_t y = oamData[i * 4];
		if (scanline >= y && scanline < (uint16_t)(y + spriteHeight))
		{
			if (spriteCount < 8)
			{
				spriteScanline[spriteCount].y = oamData[i * 4];
				spriteScanline[spriteCount].tileID = oamData[i * 4 + 1];
				spriteScanline[spriteCount].attributes = oamData[i * 4 + 2];
				spriteScanline[spriteCount].x = oamData[i * 4 + 3];

				if (i == 0)
					spriteZeroHit = true;
				spriteCount++;
			}
			else
			{
				PPUSTATUS |= 0x20;
				break;
			}
		}
	}
}

void NEW_PPU::fetchSpritePatterns()
{
	uint8_t spriteHeight = (PPUCTRL & 0x20) ? 16 : 8;

	for (int i = 0; i < spriteCount; ++i)
	{
		Sprite& sprite = spriteScanline[i];

		uint8_t tileIndex = sprite.tileID;
		uint8_t attributes = sprite.attributes;
		uint8_t yOffset = scanline - sprite.y;
		bool flipVert = attributes & 0x80;

		if (flipVert)
		{
			yOffset = spriteHeight - 1 - yOffset;
		}

		uint16_t addr = 0;

		if (spriteHeight == 16)
		{
			uint8_t table = tileIndex & 0x01;
			tileIndex &= 0xFE;

			if (yOffset >= 8)
			{
				tileIndex += 1;
				yOffset -= 8;
			}

			addr = (table * 0x1000) + tileIndex * 16 + yOffset;
		}
		else
		{
			uint16_t patternBase = (PPUCTRL & 0x08) ? 0x1000 : 0x0000;
			addr = patternBase + tileIndex * 16 + yOffset;
		}

		sprite.patternLow = readVRAM(addr);
		sprite.patternHigh = readVRAM(addr + 8);
	}
}

void NEW_PPU::renderPixel()
{
	int x = cycle - 1;
	int y = scanline;

	// Background Rendering
	uint8_t bit0 = (bgPatternShiftLow >> 15) & 1;
	uint8_t bit1 = (bgPatternShiftHigh >> 15) & 1;
	uint8_t paletteIndex = (bit1 << 1) | bit0;

	uint8_t atrr0 = (bgAttribShiftLow >> 15) & 1;
	uint8_t atrr1 = (bgAttribShiftHigh >> 15) & 1;
	uint8_t paletteHighBits = (atrr1 << 1) | atrr0;

	uint8_t paletteAddr = (paletteHighBits << 2) | paletteIndex;

	if (paletteIndex == 0)
		paletteAddr = 0x00;

	bool bgOpaque = (paletteIndex != 0);
	uint8_t bgColor = readVRAM(0x3F00 + paletteAddr);
	if (!bgOpaque) bgColor = readVRAM(0x3F00);

	// Sprite Rendering
	bool spriteVisible = false;
	uint8_t spriteColor = 0x00;
	bool spritePriority = false;

	if (PPUMASK & 0x10)  // Check if sprites are enabled
	{
		for (int i = 0; i < spriteCount; ++i)
		{
			int spriteX = spriteScanline[i].x;
			int offset = x - spriteX;
			if (offset < 0 || offset >= 8) continue;

			bool flipH = spriteScanline[i].attributes & 0x40;
			int bitIndex = flipH ? offset : (7 - offset);

			uint8_t spriteBit0 = (spriteScanline[i].patternLow >> bitIndex) & 1;
			uint8_t spriteBit1 = (spriteScanline[i].patternHigh >> bitIndex) & 1;
			uint8_t spritePixel = (spriteBit1 << 1) | spriteBit0;

			if (spritePixel == 0) continue;

			uint8_t spritePalette = spriteScanline[i].attributes & 0x03;
			spriteColor = readVRAM(0x3F10 + (spritePalette << 2) + spritePixel);
			spritePriority = !(spriteScanline[i].attributes & 0x20);
			spriteVisible = true;

			// Sprite 0 hit detection
			if (i == 0 && bgOpaque && x < 255 && (PPUMASK & 0x18))
			{
				//std::cout << "[PPU] Sprite 0 hit at scanline " << scanline << ", cycle " << cycle << std::endl;
				PPUSTATUS |= 0x40;
			}

			break;  // Use first non-transparent sprite pixel
		}
	}


	// Final Color Selection

	uint8_t finalColor = 5;
	bool bgEnabled = (PPUMASK & 0x08) && (x >= 8 || (PPUMASK & 0x02));
	bool spriteEnabled = (PPUMASK & 0x10) && (x >= 8 || (PPUMASK & 0x04));

	if (!bgEnabled && !spriteEnabled)
	{
		finalColor = readVRAM(0x3F00);  // Background color
	}
	else if (!bgEnabled)
	{
		finalColor = spriteVisible ? spriteColor : readVRAM(0x3F00);
	}
	else if (!spriteEnabled)
	{
		finalColor = bgColor;
	}
	else
	{
		if (!bgOpaque && spriteVisible)
		{
			finalColor = spriteColor;
		}
		else if (!spriteVisible)
		{
			finalColor = bgColor;
		}
		else if (spritePriority)
		{
			finalColor = spriteColor;
		}
		else
		{
			finalColor = bgColor;
		}
	}

	frameBuffer[y * 256 + x] = NESPalette[finalColor];
}

void NEW_PPU::dumpNametable()
{
	printf("NAMETABLE A: \n");

	for (int y = 0; y < 30; ++y) {       // visible tiles: 32x30
		for (int x = 0; x < 32; ++x) {
			int index = y * 32 + x;
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)nameTables[index] << " ";
		}
		std::cout << "\n";
	}

	printf("ATTRIBUTE TABLE: \n");

	for (int y = 0; y < 2; ++y)
	{
		for (int x = 0; x < 32; ++x)
		{
			int index = 960 + y * 32 + x;
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)nameTables[index] << " ";
		}
		std::cout << "\n";
	}

	printf("NAMETABLE B: \n");

	for (int y = 0; y < 30; ++y) {       // visible tiles: 32x30
		for (int x = 0; x < 32; ++x) {
			int index = 1024 + y * 32 + x;
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)nameTables[index] << " ";
		}
		std::cout << "\n";
	}

	printf("ATTRIBUTE TABLE: \n");

	for (int y = 0; y < 2; ++y)
	{
		for (int x = 0; x < 32; ++x)
		{
			int index = 1024 + 960 + y * 32 + x;
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)nameTables[index] << " ";
		}
		std::cout << "\n";
	}
	exit(0);
}