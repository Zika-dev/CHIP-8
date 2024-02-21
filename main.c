#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <string.h>
#include <SDL.h>
#include <math.h>

#define DEFAULT_FREQ 1000

const int SAMPLERATE = 44100;
const int AMPLITUDE = 28000;
const int FREQUENCY = 440;

enum registers { V0, V1, V2, V3, V4, V5, V6, V7, V8, V9, VA, VB, VC, VD, VE, VF };

enum opCodes {
	CLS = 0x0000,  // Clear the display
	RET = 0x000E,  // Return from a subroutine
	GOTO = 0x1000, // Jump to location NNN
	CALL = 0x2000, // Call subroutine at NNN
	SE = 0x3000,   // Skip next instruction if Vx = NN
	SNE = 0x4000,  // Skip next instruction if Vx != NN
	SEV = 0x5000,  // Skip next instruction if Vx = Vy
	LD = 0x6000,   // Set Vx  = NN
	ADD = 0x7000,  // Set Vx += NN
	LDV = 0x8000,  // Set Vx  = Vy
	OR = 0x8001,   // Set Vx |= Vy
	AND = 0x8002,  // Set Vx &= Vy
	XOR = 0x8003,  // Set Vx ^= Vy
	ADDV = 0x8004, // Set Vx += Vy, set VF = carry
	SUB = 0x8005,  // Set Vx -= Vy, set VF = NOT underflow
	SHR = 0x8006,  // Set Vx >>= 1, store LSB in VF
	SUBN = 0x8007, // Set Vx = Vy - Vx, set VF = NOT underflow
	SHL = 0x800E,  // Set Vx <<= 1, store MSB in VF
	SNEV = 0x9000, // Skip next instruction if Vx != Vy
	LDI = 0xA000,  // Set I = NNN
	JPV = 0xB000,  // Jump to location NNN + V0
	RND = 0xC000,  // Set Vx = | random byte | NN
	DRW = 0xD000,  // Display sprite at (Vx, Vy), set VF = collision
	SKP = 0xE09E,  // Skip next instruction if key with value Vx is pressed
	SKNP = 0xE0A1, // Skip next instruction if key with value Vx is not pressed
	LDVDT = 0xF007,// Set Vx = delay timer value
	LDK = 0xF00A,  // Wait for a key press, store the value of the key in Vx
	LDDTV = 0xF015,// Set delay timer = Vx
	LDS = 0xF018,  // Set sound timer = Vx
	ADDI = 0xF01E, // Set I += Vx
	LDF = 0xF029,  // Set I = location of sprite for digit Vx
	LDB = 0xF033,  // Store BCD representation of Vx in memory locations I, I+1, and I+2
	LDIV = 0xF055, // Store registers V0 through Vx in memory starting at location I
	LDVI = 0xF065  // Store memory starting at location I in registers V0 through Vx
};

unsigned char fontSet[80] =
{
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

struct CPU
{
	unsigned short opcode;
	unsigned char mem[4096];
	unsigned char reg[16];
	unsigned short I;
	unsigned short programC;

	unsigned short freq;

	bool graphics[64 * 32];

	unsigned char delayTimer;
	unsigned char soundTimer;

	unsigned short stack[16];
	unsigned short stackP;

	unsigned char key[16];
};

struct CPU cpu;

char mnemonic[10];
char eval[10];

void initCPU(int freq) {
	cpu.freq = freq; // Processor clock speed (Hz)
	cpu.programC = 0x200; // Program counter starts at 0x200

	cpu.I = 0;
	cpu.stackP = 0;
	cpu.delayTimer = 0;
	cpu.soundTimer = 0;

	memset(cpu.mem, 0, sizeof(cpu.mem));
	memset(cpu.reg, 0, sizeof(cpu.reg));
	memset(cpu.stack, 0, sizeof(cpu.stack));
	memset(cpu.graphics, 0, sizeof(cpu.graphics));
	memset(cpu.key, 0, sizeof(cpu.key));
}

void debugCPU() {
	printf("PC: %d\tSP: %d\tOP: %04x\t%s\t ST: %d\t DT: %d\n", cpu.programC, cpu.stackP, cpu.opcode, mnemonic, cpu.soundTimer, cpu.delayTimer, cpu.reg[0], cpu.reg[1]);
}

void memDump(int startAddress, int stopAddress) {

	if(startAddress < 0 || stopAddress > 4096) {
		printf("Error: Memory dump out of bounds\n");
		exit(1);
	}

	printf("Memory dump from 0x%.4X to 0x%.4X\n", startAddress, stopAddress);

	for(int i = startAddress; i < stopAddress; i++) {
		if(i % 16 == 0) {
			printf("\n0x%.4X ", i);
		}
		printf("%.2X ", cpu.mem[i]);
	}
}

void emulateCycle() {

	if (cpu.programC >= 0x1000 || cpu.programC < 0x200) {
		printf("Error: Program counter out of bounds\n");
		exit(2);
	}

	cpu.opcode = cpu.mem[cpu.programC] << 8 | cpu.mem[cpu.programC + 1]; // Fetch opcode

	switch (cpu.opcode & 0xF000) // Decode opcode
	{
		case 0x0000:
			switch (cpu.opcode & 0x000F)
			{
			case CLS:
				strcpy(mnemonic, "CLS");
				memset(cpu.graphics, 0, sizeof(cpu.graphics));
				cpu.programC += 2;
				break;

			case RET:
				strcpy(mnemonic, "RET");
				--cpu.stackP; // Decrement stack pointer
				cpu.programC = cpu.stack[cpu.stackP]; // Set program counter to the address at the top of the stack
				cpu.programC += 2;
				break;

			default:
				printf("\nUnknown op code: %.4X\n", cpu.opcode);
				exit(3);
			}
			break;

		case GOTO:
			strcpy(mnemonic, "GOTO");
			cpu.programC = cpu.opcode & 0x0FFF;
			break;

		case CALL:
			strcpy(mnemonic, "CALL");
			cpu.stack[cpu.stackP] = cpu.programC; // Store current address in stack
			++cpu.stackP;						  // Increment stack pointer
			cpu.programC = cpu.opcode & 0x0FFF;
			break;

		case SE:
			strcpy(mnemonic, "SE");
			if (cpu.reg[(cpu.opcode & 0x0F00) >> 8] == (cpu.opcode & 0x00FF)) {
				strcpy(eval, "equal");
				cpu.programC += 4; // Skip next instruction
			}
			else
			{
				strcpy(eval, "unequal");
				cpu.programC += 2;
			}
			break;

		case SNE:
			strcpy(mnemonic, "SNE");
			if (cpu.reg[(cpu.opcode & 0x0F00) >> 8] != (cpu.opcode & 0x00FF)) {
				strcpy(eval, "unequal");
				cpu.programC += 4; // Skip next instruction
			}
			else
			{
				strcpy(eval, "equal");
				cpu.programC += 2;
			}
			break;

		case SEV:
			strcpy(mnemonic, "SEV");
			if (cpu.reg[(cpu.opcode & 0x0F00) >> 8] == cpu.reg[(cpu.opcode & 0x00F0) >> 4]) {
				strcpy(eval, "equal");
				cpu.programC += 4; // Skip next instruction
			}
			else
			{
				strcpy(eval, "unequal");
				cpu.programC += 2;
			}
			break;

		case LD:
			strcpy(mnemonic, "LD");
			cpu.reg[(cpu.opcode & 0x0F00) >> 8] = cpu.opcode & 0x00FF;
			cpu.programC += 2;
			break;

		case ADD:
			strcpy(mnemonic, "ADD");
			cpu.reg[(cpu.opcode & 0x0F00) >> 8] += cpu.opcode & 0x00FF;
			cpu.programC += 2;
			break;

		case 0x8000:
			switch (cpu.opcode & 0xF00F)
			{
				case LDV:
					strcpy(mnemonic, "LDV");
					cpu.reg[(cpu.opcode & 0x0F00) >> 8] = cpu.reg[(cpu.opcode & 0x00F0) >> 4];
					cpu.programC += 2;
					break;

				case OR:
					strcpy(mnemonic, "OR");
					cpu.reg[(cpu.opcode & 0x0F00) >> 8] |= cpu.reg[(cpu.opcode & 0x00F0) >> 4];
					cpu.programC += 2;
					break;

				case AND:
					strcpy(mnemonic, "AND");
					cpu.reg[(cpu.opcode & 0x0F00) >> 8] &= cpu.reg[(cpu.opcode & 0x00F0) >> 4];
					cpu.programC += 2;
					break;

				case XOR:
					strcpy(mnemonic, "XOR");
					cpu.reg[(cpu.opcode & 0x0F00) >> 8] ^= cpu.reg[(cpu.opcode & 0x00F0) >> 4];
					cpu.programC += 2;
					break;

				case ADDV:
					strcpy(mnemonic, "ADDV");
					cpu.reg[(cpu.opcode & 0x0F00) >> 8] += cpu.reg[(cpu.opcode & 0x00F0) >> 4];
					if (cpu.reg[(cpu.opcode & 0x00F0) >> 4] > (0xFF - cpu.reg[(cpu.opcode & 0x0F00) >> 8])) { // Vy > (0xFF - Vx)
						cpu.reg[0xF] = 1; // Carry
					}
					else {
						cpu.reg[0xF] = 0;
					}
					cpu.programC += 2;
					break;

				case SUB:
					strcpy(mnemonic, "SUB");
					if (cpu.reg[(cpu.opcode & 0x0F00) >> 8] > cpu.reg[(cpu.opcode & 0x00F0) >> 4]) { // Vx < Vy
						cpu.reg[0xF] = 1; // No borrow
					}
					else
					{
						cpu.reg[0xF] = 0;
					}
					cpu.reg[(cpu.opcode & 0x0F00) >> 8] -= cpu.reg[(cpu.opcode & 0x00F0) >> 4];
					cpu.programC += 2;
					break;

				case SHR:
					strcpy(mnemonic, "SHR");
					cpu.reg[0xF] = cpu.reg[(cpu.opcode & 0x0F00) >> 8] & 0x1; // Store LSB in VF
					cpu.reg[(cpu.opcode & 0x0F00) >> 8] >>= 1;
					cpu.programC += 2;
					break;

				case SUBN:
					strcpy(mnemonic, "SUBN");
					if (cpu.reg[(cpu.opcode & 0x0F00) >> 8] < cpu.reg[(cpu.opcode & 0x00F0) >> 4]) { // Vx > Vy
						cpu.reg[0xF] = 1; // No borrow
					}
					else
					{
						cpu.reg[0xF] = 0;
					}
					cpu.reg[(cpu.opcode & 0x0F00) >> 8] = cpu.reg[(cpu.opcode & 0x00F0) >> 4] - cpu.reg[(cpu.opcode & 0x0F00) >> 8];
					cpu.programC += 2;
					break;

				case SHL:
					strcpy(mnemonic, "SHL");
					cpu.reg[0xF] = cpu.reg[(cpu.opcode & 0x0F00) >> 8] >> 7; // Store MSB in VF
					cpu.reg[(cpu.opcode & 0x0F00) >> 8] <<= 1;
					cpu.programC += 2;
					break;

				default:
					printf("\nUnknown op code: %.4X\n", cpu.opcode);
					exit(3);
			}
			break;
		case SNEV:
			strcpy(mnemonic, "SNEV");
			if (cpu.reg[(cpu.opcode & 0x0F00) >> 8] != cpu.reg[(cpu.opcode & 0x00F0) >> 4]) {
				cpu.programC += 4; // Skip next instruction
			}
			else
			{
				cpu.programC += 2;
			}
			break;

		case LDI:
			strcpy(mnemonic, "LDI");
			cpu.I = cpu.opcode & 0x0FFF;
			cpu.programC += 2;
			break;

		case JPV:
			strcpy(mnemonic, "JPV");
			cpu.programC = (cpu.opcode & 0x0FFF) + cpu.reg[0];
			break;

		case RND:
			strcpy(mnemonic, "RND");
			cpu.reg[(cpu.opcode & 0x0F00) >> 8] = (rand() % 256) & (cpu.opcode & 0x00FF); // Random number (0-255) & NN
			cpu.programC += 2;
			break;

		case DRW:
			strcpy(mnemonic, "DRW");
			unsigned short x = cpu.reg[(cpu.opcode & 0x0F00) >> 8]; // X coordinate
			unsigned short y = cpu.reg[(cpu.opcode & 0x00F0) >> 4]; // Y coordinate

			unsigned short h = cpu.opcode & 0x000F;					// Height
			unsigned short pixel;									// Row of pixel data

			cpu.reg[0xF] = 0; // Reset collision flag

			for(int i = 0; i < h; i++) { // Height
				pixel = cpu.mem[cpu.I + i]; // Current pixel row
				for(int j = 0; j < 8; j++) { // Width
					if((pixel & (0x80 >> j)) != 0) { // Check if the pixel is set
						if(cpu.graphics[(x + j + ((y + i) * 64))]) { // Check if the pixel is already set
							cpu.reg[0xF] = 1; // Collision
						}
						if (x + j + ((y + i) * 64) > 2048) {
							printf("Error: Pixel: %d out of bounds\n", x + j + ((y + i) * 64));
						}
						else
						{
							cpu.graphics[x + j + ((y + i) * 64)] ^= 1; // XOR the pixel
						}
						
					}
				}
			}
			cpu.programC += 2;
			break;

		case 0xE000:
			switch (cpu.opcode & 0xF0FF)
			{
				case SKP:
					strcpy(mnemonic, "SKP");
					if (cpu.key[cpu.reg[(cpu.opcode & 0x0F00) >> 8]] != 0) {
						cpu.programC += 4; // Skip next instruction
					}
					else
					{
						cpu.programC += 2;
					}
					break;

				case SKNP:
					if (cpu.key[cpu.reg[(cpu.opcode & 0x0F00) >> 8]] == 0) {
						cpu.programC += 4; // Skip next instruction
					}
					else
					{
						cpu.programC += 2;
					}
					break;
				default:
					printf("\nUnknown op code: %.4X\n", cpu.opcode);
					exit(3);
			}
		break;

		case 0xF000:
			switch (cpu.opcode & 0xF0FF){
				case LDVDT:
					strcpy(mnemonic, "LDVDT");
					cpu.reg[(cpu.opcode & 0x0F00) >> 8] = cpu.delayTimer;
					cpu.programC += 2;
					break;

				case LDK:
					strcpy(mnemonic, "LDK");
					bool keyPress = false;

					for (int i = 0; i < 16; ++i) {
						if (cpu.key[i] == 1) {
							keyPress = true;
							cpu.reg[(cpu.opcode & 0x0F00) >> 8] = i;
						}
					}
					if (!keyPress) {
						return;
					}
					cpu.programC += 2;
					break;

				case LDDTV:
					strcpy(mnemonic, "LDDTV");
					cpu.delayTimer = cpu.reg[(cpu.opcode & 0x0F00) >> 8];
					cpu.programC += 2;
					break;

				case LDS:
					strcpy(mnemonic, "LDS");
					cpu.soundTimer = cpu.reg[(cpu.opcode & 0x0F00) >> 8];
					cpu.programC += 2;
					break;

				case ADDI:
					strcpy(mnemonic, "ADDI");
					cpu.I += cpu.reg[(cpu.opcode & 0x0F00) >> 8];
					cpu.programC += 2;
					break;

				case LDF:
					strcpy(mnemonic, "LDF");
					cpu.I = cpu.reg[(cpu.opcode & 0x0F00) >> 8] * 0x5; // Used for font sprites which are 5 pixels tall
					cpu.programC += 2;
					break;

				case LDB:
					strcpy(mnemonic, "LDB");
					cpu.mem[cpu.I]     = cpu.reg[(cpu.opcode & 0x0F00) >> 8] / 100;	      // Hundreds digit
					cpu.mem[cpu.I + 1] = (cpu.reg[(cpu.opcode & 0x0F00) >> 8] / 10) % 10; // Tens digit
					cpu.mem[cpu.I + 2] = cpu.reg[(cpu.opcode & 0x0F00) >> 8] % 10;	      // Ones digit
					cpu.programC += 2;
					break;

				case LDIV:
					strcpy(mnemonic, "LDIV");
					for (int i = 0; i <= ((cpu.opcode & 0x0F00) >> 8); ++i) {
						cpu.mem[cpu.I + i] = cpu.reg[i];
					}
					cpu.programC += 2;
					break;

				case LDVI:
					strcpy(mnemonic, "LDVI");
					for (int i = 0; i <= ((cpu.opcode & 0x0F00) >> 8); ++i) {
						cpu.reg[i] = cpu.mem[cpu.I + i];
					}
					cpu.programC += 2;
					break;

				default:
					printf("\nUnknown op code: %.4X\n", cpu.opcode);
					exit(3);
			}
		default:
			break;
	}
}

void printUsage() {
	printf("Usage: CHIP-8 [options] rom_file\n");
	printf("Options:\n");
	printf("  -s <speed>     Set CPU speed (default: 1000 Hz)\n");
	printf("  -d             Enable debugging\n");
	printf("  -hex <start> <stop>  Hexdump memory from start to stop\n");
	printf("  -m Mutes audio\n");
}

static int wavePosition = 0;
bool playAudio = true;

void audioCallback(void* userdata, Uint8* stream, int len) {
	Sint16* buffer = (Sint16*)stream;
	int samples = len / 2; // 2 bytes per sample (S16 format)

	if (cpu.soundTimer != 0 && playAudio) {
		for (int i = 0; i < samples; i += 2) {
			double time = wavePosition / (double)SAMPLERATE;
			Sint16 sampleValue = (Sint16)(AMPLITUDE * sin(2 * M_PI * FREQUENCY * time));

			// Write sample to both left & right channels
			buffer[i] = sampleValue; // Left channel
			buffer[i + 1] = sampleValue; // Right channel

			wavePosition++;
		}
	}
	else {
		memset(stream, 0, len); // Output silence
	}
}

int main(int argc[], char* argv[])
{	
	if(argc < 2) {
		printUsage();
		return 1;
	}

	bool debug = false;
	bool doMemDump = false; int startAddress = 0; int stopAddress = 0;
	int newFreq = DEFAULT_FREQ;

	FILE* rom = NULL;

	// Parse command line arguments
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-s") == 0) {
			newFreq = atoi(argv[i + 1]);

			if(newFreq == 0) {
				printf("Error: Invalid speed\n");
				return 1;
			}

			++i;
		}
		else if (strcmp(argv[i], "-d") == 0) {
			debug = true;
		}
		else if (strcmp(argv[i], "-mem") == 0) {
			doMemDump = true;

			if (i + 2 < argc) {

				// If argv stars with 0x, use base 16
				if (argv[i + 1][0] == '0' && argv[i + 1][1] == 'x') {
					startAddress = strtol(argv[i + 1], NULL, 16);
					stopAddress = strtol(argv[i + 2], NULL, 16);
				}
				else
				{
					startAddress = atoi(argv[i + 1]);
					stopAddress = atoi(argv[i + 2]);
				}
				i += 2;
			}
			else {
				printf("Error: -mem option requires start and stop addresses\n");
				return 1;
			}
		}
		else if (strcmp(argv[i], "-m") == 0) {
			playAudio = false;
		}
		else
		{
			if (!rom) {
				rom = fopen(argv[i], "rb");
				if (!rom) {
					printf("Error: Unable to open file: %s\n", argv[i]);
					return 1;
				}
			}
			else
			{
				printf("Error: Invalid command: %s\n", argv[i]);
				return 1;
			}
		}
	}

	initCPU(newFreq);
	
	fread(&cpu.mem[0x200], 1, 0x1000 - 0x200, rom);
	fclose(rom);

	if (doMemDump) {
		memDump(startAddress, stopAddress);
		return;
	}

	// Load fontset into memory
	for (int i = 0; i < 80; i++) {
		cpu.mem[i] = fontSet[i];
	}

	// Set up the render system
	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;

	if (SDL_Init(SDL_INIT_VIDEO) != 0 || SDL_Init(SDL_INIT_AUDIO) != 0) {
		printf("Error: Unable to initialize SDL: %s\n", SDL_GetError());
		return 1;
	};

	SDL_CreateWindowAndRenderer(64*10, 32*10, 0, &window, &renderer);

	// Set windowTitle to be name of ROM and processor speed
	char windowTitle[50];
	sprintf(windowTitle, "CHIP-8 Emulator - %s @ %dHz", argv[1], cpu.freq);

	SDL_SetWindowTitle(window, windowTitle);
	SDL_RenderSetScale(renderer, 10, 10);

	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

	// Setup audio
	SDL_AudioSpec audioSpec;
	audioSpec.freq = 44100;  // Sample rate
	audioSpec.format = AUDIO_S16SYS;  // Audio format (16-bit signed, system byte order)
	audioSpec.channels = 2;  // Mono
	audioSpec.samples = 1024;  // Buffer size
	audioSpec.callback = audioCallback;
	audioSpec.userdata = NULL;

	if (SDL_OpenAudio(&audioSpec, NULL) < 0) {
		printf("SDL could not open audio! SDL_Error: %s\n", SDL_GetError());
		// Handle error
	}

	SDL_PauseAudio(0);

	// Setup keyboard mapping
	const int numKeys = 16;
	const SDL_Keycode keys[16] = {
	SDLK_x, SDLK_1, SDLK_2, SDLK_3,
	SDLK_q, SDLK_w, SDLK_e, SDLK_a,
	SDLK_s, SDLK_d, SDLK_z, SDLK_c,
	SDLK_4, SDLK_r, SDLK_f, SDLK_v
	};

	int quit = 0;
	SDL_Event event;

	double frameTime = 1000 / 60;
	Uint32 lastTime = SDL_GetTicks();

	while (!quit) {
		// Handle input
		while (SDL_PollEvent(&event) != 0) {
			switch (event.type)
			{
				case SDL_QUIT:
					quit = 1;
					break;

				case SDL_KEYDOWN:
					for(int i = 0; i < numKeys; i++) {
						if(event.key.keysym.sym == keys[i]) {
							cpu.key[i] = 1;
						}
					}
					break;

				case SDL_KEYUP:
					for(int i = 0; i < numKeys; i++) {
						if(event.key.keysym.sym == keys[i]) {
							cpu.key[i] = 0;
						}
					}
					break;

				default:
					break;
			}
		}
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		// Draw graphics
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		for(int i = 0; i < 64; i++) {
			for(int j = 0; j < 32; j++) {
				if(cpu.graphics[i + j * 64]) {
					SDL_RenderDrawPoint(renderer, i, j);
				}
			}
		}

		// Decrement timers once every 60Hz
		Uint32 currentTime = SDL_GetTicks();
		Uint32 deltaTime = currentTime - lastTime;

		if (deltaTime >= frameTime) {
			if(cpu.delayTimer > 0) {
				--cpu.delayTimer;
			}
			if (cpu.soundTimer > 0) {
				--cpu.soundTimer;
			}
			lastTime = currentTime;
		}

		emulateCycle();

		if (debug) {
			debugCPU();
		}

		SDL_RenderPresent(renderer);
		SDL_Delay(1000 / cpu.freq);
	}
	
	return 0;
}