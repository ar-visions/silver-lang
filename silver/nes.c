#include <A>

#define PRG_ROM_BANK_SIZE 16384  // 16KB per PRG-ROM bank
#define CHR_ROM_BANK_SIZE 8192   // 8KB per CHR-ROM bank

// Define enums for NES opcodes and registers
#define NESOpcode_schema(X, Y) \
    enum_value(X, Y, ADC)   /* Add with Carry */ \
    enum_value(X, Y, AND)   /* Logical AND */ \
    enum_value(X, Y, ASL)   /* Arithmetic Shift Left */ \
    enum_value(X, Y, BCC)   /* Branch if Carry Clear */ \
    enum_value(X, Y, BCS)   /* Branch if Carry Set */ \
    enum_value(X, Y, BEQ)   /* Branch if Equal */ \
    enum_value(X, Y, BIT)   /* Bit Test */ \
    enum_value(X, Y, BMI)   /* Branch if Minus */ \
    enum_value(X, Y, BNE)   /* Branch if Not Equal */ \
    enum_value(X, Y, BPL)   /* Branch if Positive */ \
    enum_value(X, Y, BRK)   /* Force Interrupt */ \
    enum_value(X, Y, BVC)   /* Branch if Overflow Clear */ \
    enum_value(X, Y, BVS)   /* Branch if Overflow Set */ \
    enum_value(X, Y, CLC)   /* Clear Carry Flag */ \
    enum_value(X, Y, CLD)   /* Clear Decimal Mode */ \
    enum_value(X, Y, CLI)   /* Clear Interrupt Disable */ \
    enum_value(X, Y, CLV)   /* Clear Overflow Flag */ \
    enum_value(X, Y, CMP)   /* Compare Accumulator */ \
    enum_value(X, Y, CPX)   /* Compare X Register */ \
    enum_value(X, Y, CPY)   /* Compare Y Register */ \
    enum_value(X, Y, DEC)   /* Decrement Memory */ \
    enum_value(X, Y, DEX)   /* Decrement X Register */ \
    enum_value(X, Y, DEY)   /* Decrement Y Register */ \
    enum_value(X, Y, EOR)   /* Exclusive OR */
declare_enum(NESOpcode)

#define NESRegister_schema(T, ARG) \
    enum_value(T, ARG, A)   /* Accumulator */ \
    enum_value(T, ARG, X)   /* X Register */ \
    enum_value(T, ARG, Y)   /* Y Register */ \
    enum_value(T, ARG, SP)  /* Stack Pointer */ \
    enum_value(T, ARG, PC)  /* Program Counter */ \
    enum_value(T, ARG, P)   /* Status Register */
declare_enum(NESRegister)

// Declare class structures
typedef struct CPU* CPU;
typedef struct PPU* PPU;
typedef struct Memory* Memory;
typedef struct NES* NES;

// CPU class
#define CPU_schema(X,Y) \
    i_prop(X,Y, public, AType,      nes)      \
    i_prop(X,Y, public, i8,         a)        /* Accumulator */ \
    i_prop(X,Y, public, i8,         x)        /* X register */ \
    i_prop(X,Y, public, i8,         y)        /* Y register */ \
    i_prop(X,Y, public, i8,         sp)       /* Stack Pointer */ \
    i_prop(X,Y, public, i8,         status)   /* Status flags */ \
    i_prop(X,Y, public, u16,        pc)       /* Program counter */ \
    i_prop(X,Y, intern, u8*,        memory)   /* Memory space (RAM/ROM) */ \
    i_method(X,Y, public, none,     step)     /* Execute one CPU step */ \
    i_method(X,Y, public, none,     reset)    /* Reset the CPU */
declare_class(CPU)

// PPU class
#define PPU_schema(X,Y) \
    i_prop(X,Y, public, AType,      nes)      \
    i_prop(X,Y, public, i8,         control)  /* Control register */ \
    i_prop(X,Y, public, i8,         status)   /* Status register */ \
    i_prop(X,Y, public, array,      vram)     /* Video RAM */ \
    i_prop(X,Y, public, i8,         oam)      /* Object Attribute Memory */ \
    i_method(X,Y, public, none,     step)     /* Execute one PPU step */
declare_class(PPU)

// Memory class
#define Memory_schema(X,Y) \
    i_prop(X,Y, intern, u8*,        ram)      /* System RAM */ \
    i_prop(X,Y, intern, u8*,        prg_rom)  /* Cartridge PRG-ROM */ \
    i_prop(X,Y, intern, u8*,        chr_rom)  /* Cartridge CHR-ROM */
declare_class(Memory)

// NES class
#define NES_schema(X,Y) \
    i_prop(X,Y, public, CPU,        cpu)      \
    i_prop(X,Y, public, PPU,        ppu)      \
    i_prop(X,Y, public, Memory,     memory)   \
    i_method(X,Y, public, none,     run)      /* Run the emulator */ \
    i_method(X,Y, public, none,     load_rom, path)  /* Load ROM */
declare_class(NES)

none PPU_reset(PPU ppu) { }
none CPU_reset(CPU cpu) { }

none PPU_step(PPU ppu) { }
// CPU step function
none CPU_step(CPU cpu) {
    // Fetch, decode, and execute a single instruction
    u8 opcode = cpu->memory[cpu->pc];
    cpu->pc++;  // Increment the program counter

    // Placeholder for opcode handling
    switch (opcode) {
        case 0xA9:  // LDA immediate
            cpu->a = cpu->memory[cpu->pc];
            cpu->pc++;
            break;
        // Add more opcodes here
        default:
            print("Unknown opcode: 0x%X", opcode);
            break;
    }
}

// NES run function
none NES_run(NES nes) {
    while (true) {
        // Execute CPU and PPU cycles
        call(nes->cpu, step);
        call(nes->ppu, step);

        // Handle interrupts, I/O, etc.
    }
}

// Structure to hold the iNES header
typedef struct iNESHeader {
    char     magic[4];       // "NES\x1A"
    uint8_t  prg_rom_size;   // Number of 16KB PRG-ROM banks
    uint8_t  chr_rom_size;   // Number of 8KB CHR-ROM banks
    uint8_t  flags_6;        // Flags (mirroring, battery-backed, etc.)
    uint8_t  flags_7;        // Mapper number and other flags
    uint8_t  unused[8];      // Unused/reserved bytes
} iNESHeader;

// Function to load a ROM into the NES system
none NES_load_rom(NES nes, path rom) {
    FILE *rom_file = fopen(rom->chars, "rb");
    if (!rom_file) {
        printf("Failed to load ROM: %s\n", rom->chars);
        return;
    }

    // Read the iNES header
    iNESHeader header;
    fread(&header, sizeof(iNESHeader), 1, rom_file);

    // Validate the magic number (should be "NES\x1A")
    if (memcmp(header.magic, "NES\x1A", 4) != 0) {
        printf("Invalid ROM file: incorrect magic number.\n");
        fclose(rom_file);
        return;
    }

    // Get the number of PRG and CHR ROM banks
    size_t prg_size = header.prg_rom_size * PRG_ROM_BANK_SIZE;
    size_t chr_size = header.chr_rom_size * CHR_ROM_BANK_SIZE;

    nes->memory = new(Memory);
    nes->cpu    = new(CPU);
    nes->ppu    = new(PPU);

    // Load the PRG-ROM (program ROM) into memory
    nes->memory->prg_rom = (uint8_t *)calloc(prg_size, sizeof(uint8_t));
    fread(nes->memory->prg_rom, sizeof(uint8_t), prg_size, rom_file);

    // Load the CHR-ROM (character ROM) if present
    if (chr_size > 0) {
        nes->memory->chr_rom = (uint8_t *)calloc(chr_size, sizeof(uint8_t));
        fread(nes->memory->chr_rom, sizeof(uint8_t), chr_size, rom_file);
    } else {
        nes->memory->chr_rom = NULL; // Some games use CHR-RAM instead of ROM
    }

    // Close the ROM file
    fclose(rom_file);

    printf("ROM loaded successfully: %s\n", rom->chars);
    printf("PRG-ROM size: %lu bytes\n", prg_size);
    printf("CHR-ROM size: %lu bytes\n", chr_size);
}

define_class(NES)
define_class(CPU)
define_class(PPU)
define_class(Memory)

int main(int argc, char **argv) {
    A_start();
    AF     pool     = allocate(AF);
    map    defaults = map_of("rom", new(path, chars, "/home/kalen/Downloads/megaman2.nes"), null);
    string ikey     = str("rom");
    map    args     = A_args(argc, argv, defaults, ikey);
    path   rom      = call(args, get, ikey);
    assert (call(rom, exists), "rom %o does not exist", rom);

    NES nes = new(NES);
    call(nes, load_rom, rom);
    call(nes, run);

    drop(pool);
}
