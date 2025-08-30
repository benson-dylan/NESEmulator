#include <iostream>
#include <iomanip>
#include <SDL.h>
#include "cpu.h"
#include "cartridge.h"
#include "memory.h"
#include "ppu.h"
#include "new_ppu.h"
#include "apu.h"

using namespace std;

void renderFrame(SDL_Renderer* renderer, SDL_Texture* screenTex, uint32_t* frameBuffer);
void viewNametable(SDL_Renderer* renderer, SDL_Texture* screenTex, NEW_PPU& ppu, uint16_t base);

int main(int argc, char* argv[])
{
    // Value determines size of window
    int scale = 3;
	bool viewNametable0 = false;
	bool viewNametable1 = false;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        std::cout << "Failed to initialize the SDL2 library\n";
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL2 Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        256 * scale, 224 * scale,
        SDL_WINDOW_SHOWN);

    if (!window)
    {
        std::cout << "Failed to create window\n";
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 
                                                SDL_RENDERER_ACCELERATED | 
                                                SDL_RENDERER_PRESENTVSYNC);

    if (!renderer)
    {
        std::cout << "Failed to create renderer\n";
        std::cout << "SDL2 Error: " << SDL_GetError() << "\n";
        return -1;
	}

    SDL_Texture* screenTex = SDL_CreateTexture(renderer,
                                               SDL_PIXELFORMAT_RGBA8888,
                                               SDL_TEXTUREACCESS_STREAMING,
                                               256, 240);

    if (!screenTex)
    {
        std::cout << "Failed to create texture\n";
        std::cout << "SDL2 Error: " << SDL_GetError() << "\n";
		return -1;
    }

    bool keep_window_open = true;

    // Pixel array that gets written to every frame
    // For debugging if screen is being rendered to
    // PPU has own frame buffer
	std::array<uint32_t, 256 * 240> frameBuffer;

    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 256; x++) {
            frameBuffer[y * 256 + x] = 0xFF0000FF; // Solid red
        }
    }

    // NES COMPONENTS
    Cartridge cartridge;

    if (!cartridge.loadROM("../ROMS/DK.nes"))
    {
        std::cout << "ROM not loaded.\n";
        return -1;
    }

    // Cartridge is passed to the PPU and RAM both for memory purposes
    // PPU contains VRAM and CPU has no onboard memory
    // Cartridge contains some onboard memory as well
    
    //PPU ppu(&cartridge);
    NEW_PPU ppu(&cartridge);

    //APU apu;

    Memory memory(&cartridge, &ppu);
    // Has access to RAM and minimal access to PPU
    CPU cpu(&memory, &ppu);

    // Main render loop
    while (keep_window_open)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e) > 0)
        {
            switch (e.type)
            {
                case SDL_QUIT:
                    keep_window_open = false;
                    break;
				case SDL_KEYDOWN:
                    switch (e.key.keysym.sym)
                    {
                        case SDLK_F1:
                            viewNametable0 = !viewNametable0;
                            viewNametable1 = false;
							cout << "Nametable 0" << (viewNametable0 ? " enabled" : " disabled") << endl;
							break;
                        case SDLK_F2:
                            viewNametable1 = !viewNametable1;
                            viewNametable0 = false;
							cout << "Nametable 1" << (viewNametable1 ? " enabled" : " disabled") << endl;
							break;
                        default:
                            break;
					}


            }
        }

        // One CPU operation
        // Triggers PPU step internally, 1 CPU step = 3 PPU Steps
        cpu.step();

        if (ppu.getNMI())
        {
            cpu.handleNMI();
        }

        // Frame rendered to screen after being marked as complete
        if (viewNametable0)
        {
            viewNametable(renderer, screenTex, ppu, 0x0000);
        }
        else if (viewNametable1)
        {
            viewNametable(renderer, screenTex, ppu, 0x1000);
        }
        else if (ppu.isFrameComplete())
        {
            renderFrame(renderer, screenTex, ppu.getFrameBuffer());
            //renderFrame(renderer, screenTex, const_cast<uint32_t*>(frameBuffer.data()));
            ppu.resetFrameComplete();
        }
    }
    SDL_DestroyTexture(screenTex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

void renderFrame(SDL_Renderer* renderer, SDL_Texture* screenTex, uint32_t* frameBuffer)
{
    SDL_UpdateTexture(screenTex, nullptr, frameBuffer, 256 * sizeof(uint32_t));
	SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, screenTex, nullptr, nullptr);
	SDL_RenderPresent(renderer);
}

static const uint32_t palette[64] = {
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

void viewNametable(SDL_Renderer* renderer, SDL_Texture* screenTex, NEW_PPU& ppu, uint16_t base)
{
    std::array<uint32_t, 256 * 240> frameBuffer;
    const int tilesPerRow = 16;

    for (int i = 0; i < 256; i++)
    {
        int tileX = (i % tilesPerRow) * 8;
        int tileY = (i / tilesPerRow) * 8;

        uint16_t tileAddr = base + i * 16;

        for (int row = 0; row < 8; ++row) {
            uint8_t plane0 = ppu.readVRAM(tileAddr + row);
            uint8_t plane1 = ppu.readVRAM(tileAddr + row + 8);

            for (int col = 0; col < 8; ++col) {
                uint8_t bit0 = (plane0 >> (7 - col)) & 1;
                uint8_t bit1 = (plane1 >> (7 - col)) & 1;
                uint8_t pixel = (bit1 << 1) | bit0;

                // Map to a debug grayscale palette: 0–3
                uint32_t color = palette[pixel];
                
                // Calculate the position in the framebuffer
                int x = tileX + col;
                int y = tileY + row;
                if (x < 256 && y < 240) {
                    frameBuffer[y * 256 + x] = color;
                }
            }
        }
    }

    // Render the frame buffer to screen
    renderFrame(renderer, screenTex, frameBuffer.data());
}