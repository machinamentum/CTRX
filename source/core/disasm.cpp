/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 94):
 * <joshuahuelsman@gmail.com> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Josh Huelsman
 * ----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include "mips.h"
#include "disasm.h"

static void
UnknownOpCode(disasm_opcode_info *Op)
{
    // TODO exceptions & coprocessor0
    //    DumpState(Cpu, Op);
    printf("Unknown Op: 0x%02lX:0x%02lX", Op->Select0, Op->Select1);
}


typedef void (*jt_func)(disasm_opcode_info *);

static jt_func PrimaryJumpTable[0x40];
static jt_func SecondaryJumpTable[0x40];
static const char *RNT[34] =
{
    "zr",
    "at",

    "v0",
    "v1",

    "a0",
    "a1",
    "a2",
    "a3",

    "t0",
    "t1",
    "t2",
    "t3",
    "t4",
    "t5",
    "t6",
    "t7",

    "s0",
    "s1",
    "s2",
    "s3",
    "s4",
    "s5",
    "s6",
    "s7",
    "t8",
    "t9",

    "k0",
    "k1",

    "gp",
    "sp",
    "fp",

    "ra",
    "pc",

    "hi:lo"
};

static const char *C0RNT[17] =
{
    "r0",
    "r1",
    "r2",
    "bpc",
    "r4",
    "bda",
    "jumpdest",
    "dcic",
    "bva",
    "bdam",
    "r10",
    "bpcm",
    "sr",
    "cause",
    "epc",
    "prid",
};

void
DumpState(MIPS_R3000 *Cpu)
{
    for (int i = 0; i < 32; ++i)
    {
        printf("%s: 0x%08lX  ", RNT[i], Cpu->registers[i]);
        if (i % 2 == 1) printf("\n");
    }
    printf("PC : 0x%08lX\n", Cpu->pc);
}

static char ScratchString[5];

inline const char *
C0Name(u8 Reg)
{
    if (Reg > C0_PRID)
    {
        snprintf(ScratchString, 4, "r%d",  Reg);
        return ScratchString;
    }

    return C0RNT[Reg];
}

inline const char *
CNName(u8 Reg)
{
    snprintf(ScratchString, 3, "r%d",  Reg);
    return ScratchString;
}

void
DumpC0State(Coprocessor *C0)
{
    for (int i = 0; i < 32; ++i)
    {
        printf("%s: 0x%08lX  ", C0Name(i), C0->registers[i]);
        if (i % 2 == 1) printf("\n");
    }
}

//Exceptions
static void
SysCall(disasm_opcode_info *OpCode)
{
    printf("syscall");
    u32 Immediate = OpCode->Immediate;
    if (Immediate)
    {
        printf(" 0x%08lX", Immediate);
    }
    printf("\n");
}

static void
Break(disasm_opcode_info *OpCode)
{
    printf("break");
    u32 Immediate = OpCode->Immediate;
    if (Immediate)
    {
        printf(" 0x%08lX", Immediate);
    }
}

//Arithmetic
static void
AddU(disasm_opcode_info *OpCode)
{
    if (OpCode->RightValue)
    {
        printf("addu %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
    }
    else
    {
        printf("move %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue]);
    }

}

static void
AddIU(disasm_opcode_info *OpCode)
{
    printf("addiu %s, %s, 0x%04lX", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], OpCode->Immediate);
}

static void
SubU(disasm_opcode_info *OpCode)
{
    printf("subu %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

static void
Add(disasm_opcode_info *OpCode)
{
    printf("add %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

static void
AddI(disasm_opcode_info *OpCode)
{
    printf("addi %s, %s, 0x%04lX", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], OpCode->Immediate);
}

static void
Sub(disasm_opcode_info *OpCode)
{
    printf("sub %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

//HI:LO operations
static void
MFHI(disasm_opcode_info *OpCode)
{
    printf("mfhi %s", RNT[OpCode->DestinationRegister]);
}

static void
MFLO(disasm_opcode_info *OpCode)
{
    printf("mflo %s", RNT[OpCode->DestinationRegister]);
}

static void
MTHI(disasm_opcode_info *OpCode)
{
    printf("mthi %s", RNT[OpCode->LeftValue]);
}

static void
MTLO(disasm_opcode_info *OpCode)
{
    printf("mtlo %s", RNT[OpCode->LeftValue]);
}

static void
Mult(disasm_opcode_info *OpCode)
{
    printf("mult %s, %s", RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

static void
MultU(disasm_opcode_info *OpCode)
{
    printf("multu %s, %s", RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

static void
Div(disasm_opcode_info *OpCode)
{
    printf("div %s, %s", RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

static void
DivU(disasm_opcode_info *OpCode)
{
    printf("divu %s, %s", RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

//Store
static void
SW(disasm_opcode_info *OpCode)
{
    printf("sw %s, 0x%04lX(%s)", RNT[OpCode->RightValue], OpCode->Immediate, RNT[OpCode->LeftValue]);
}

static void
SH(disasm_opcode_info *OpCode)
{
    printf("sh %s, 0x%04lX(%s)", RNT[OpCode->RightValue], OpCode->Immediate, RNT[OpCode->LeftValue]);
}

static void
SB(disasm_opcode_info *OpCode)
{
    printf("sb %s, 0x%04lX(%s)", RNT[OpCode->RightValue], OpCode->Immediate, RNT[OpCode->LeftValue]);
}

//Load
static void
LUI(disasm_opcode_info *OpCode)
{
    printf("lui %s, 0x%04lX", RNT[OpCode->DestinationRegister], OpCode->Immediate);
}

static void
LW(disasm_opcode_info *OpCode)
{
    printf("lw %s, 0x%04lX(%s)", RNT[OpCode->DestinationRegister], OpCode->Immediate, RNT[OpCode->LeftValue]);
}

static void
LBU(disasm_opcode_info *OpCode)
{
    printf("lbu %s, 0x%04lX(%s)", RNT[OpCode->DestinationRegister], OpCode->Immediate, RNT[OpCode->LeftValue]);
}

static void
LHU(disasm_opcode_info *OpCode)
{
    printf("lhu %s, 0x%04lX(%s)", RNT[OpCode->DestinationRegister], OpCode->Immediate, RNT[OpCode->LeftValue]);
}

static void
LB(disasm_opcode_info *OpCode)
{
    printf("lb %s, 0x%04lX(%s)", RNT[OpCode->DestinationRegister], OpCode->Immediate, RNT[OpCode->LeftValue]);
}

static void
LH(disasm_opcode_info *OpCode)
{
    printf("lh %s, 0x%04lX(%s)", RNT[OpCode->DestinationRegister], OpCode->Immediate, RNT[OpCode->LeftValue]);
}


// Jump/Call
static void
J(disasm_opcode_info *OpCode)
{
    printf("j 0x%08lX", OpCode->Immediate * 4);
}

static void
JAL(disasm_opcode_info *OpCode)
{
    printf("jal 0x%08lX", OpCode->Immediate * 4);
}

static void
JR(disasm_opcode_info *OpCode)
{
    printf("jr %s", RNT[OpCode->LeftValue]);
}

static void
JALR(disasm_opcode_info *OpCode)
{
    printf("jalr %s, %s", RNT[OpCode->RADestinationRegister], RNT[OpCode->LeftValue]);
}

static void
BranchZero(disasm_opcode_info *OpCode)
{
    //bltz, bgez, bltzal, bgezal
    u8 type = OpCode->RightValue;
    if (type & 0b00001)
    {
        //bgez
        printf("bgez");
        if (type & 0b10000)
        {
            printf("al");
        }
    }
    else
    {
        //bltz
         printf("bltz");
        if (type & 0b10000)
        {
            printf("al");
        }
    }
    printf(" %s, 0x%04lX", RNT[OpCode->LeftValue], OpCode->Immediate);
}

static void
BEQ(disasm_opcode_info *OpCode)
{
    printf("beq %s, %s, 0x%08lX", RNT[OpCode->LeftValue], RNT[OpCode->RightValue], OpCode->CurrentAddress + 4 + OpCode->Immediate * 4);
}

static void
BNE(disasm_opcode_info *OpCode)
{
    printf("bne %s, %s, 0x%08lX", RNT[OpCode->LeftValue], RNT[OpCode->RightValue], OpCode->CurrentAddress + 4 + OpCode->Immediate * 4);
}

static void
BLEZ(disasm_opcode_info *OpCode)
{
    printf("blez %s, 0x%08lX", RNT[OpCode->LeftValue], OpCode->CurrentAddress + 4 + OpCode->Immediate * 4);
}

static void
BGTZ(disasm_opcode_info *OpCode)
{
    printf("bgtz %s, 0x%08lX", RNT[OpCode->LeftValue], OpCode->CurrentAddress + 4 + OpCode->Immediate * 4);

}

//Logical
static void
AndI(disasm_opcode_info *OpCode)
{
    printf("andi %s, %s, 0x%04lX", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], OpCode->Immediate & 0xFFFF);
}

static void
OrI(disasm_opcode_info *OpCode)
{
    printf("ori %s, %s, 0x%04lX", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], OpCode->Immediate & 0xFFFF);
}

static void
And(disasm_opcode_info *OpCode)
{
    printf("and %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

static void
Or(disasm_opcode_info *OpCode)
{
    printf("or %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

static void
XOr(disasm_opcode_info *OpCode)
{
    printf("xor %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}
static void
NOr(disasm_opcode_info *OpCode)
{
    printf("nor %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

static void
XOrI(disasm_opcode_info *OpCode)
{
    printf("xori %s, %s, 0x%04lX", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], OpCode->Immediate & 0xFFFF);
}

//shifts
static void
SLLV(disasm_opcode_info *OpCode)
{
    printf("sllv %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->RightValue], RNT[OpCode->LeftValue]);
}

static void
SRLV(disasm_opcode_info *OpCode)
{
    printf("srlv %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->RightValue], RNT[OpCode->LeftValue]);
}

static void
SRAV(disasm_opcode_info *OpCode)
{
    printf("srav %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->RightValue], RNT[OpCode->LeftValue]);
}

static void
SLL(disasm_opcode_info *OpCode)
{
    if (OpCode->DestinationRegister | OpCode->RightValue | OpCode->Immediate)
    {
        printf("sll %s, %s, 0x%02lX", RNT[OpCode->DestinationRegister], RNT[OpCode->RightValue], OpCode->Immediate);
    }
    else
    {
        printf("nop");
    }
}

static void
SRL(disasm_opcode_info *OpCode)
{
    printf("srl %s, %s, 0x%02lX", RNT[OpCode->DestinationRegister], RNT[OpCode->RightValue], OpCode->Immediate);
}

static void
SRA(disasm_opcode_info *OpCode)
{
    printf("sra %s, %s, 0x%02lX", RNT[OpCode->DestinationRegister], RNT[OpCode->RightValue], OpCode->Immediate);
}

// comparison
static void
SLT(disasm_opcode_info *OpCode)
{
    printf("slt %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

static void
SLTU(disasm_opcode_info *OpCode)
{
    printf("sltu %s, %s, %s", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], RNT[OpCode->RightValue]);
}

static void
SLTI(disasm_opcode_info *OpCode)
{
    printf("slti %s, %s, 0x%04lX", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], OpCode->Immediate);
}

static void
SLTIU(disasm_opcode_info *OpCode)
{
    printf("sltiu %s, %s, 0x%04lX", RNT[OpCode->DestinationRegister], RNT[OpCode->LeftValue], OpCode->Immediate);
}


// coprocessor ops
static void
COP0(disasm_opcode_info *OpCode)
{
    switch (OpCode->FunctionSelect) {
        case 0b00000:
            printf("mfc0 %s, %s", RNT[OpCode->DestinationRegister], C0Name(OpCode->LeftValue));
            break;
        case 0b00010:
            printf("cfc0 %s, %s", RNT[OpCode->DestinationRegister], C0Name(OpCode->LeftValue - 32));
            break;
        case 0b00100:
            printf("mtc0 %s, %s", RNT[OpCode->LeftValue], C0Name(OpCode->DestinationRegister));
            break;
        case 0b00110:
            printf("ctc0 %s, %s", RNT[OpCode->LeftValue], C0Name(OpCode->DestinationRegister - 32));
            break;
        case 0b01000:
        {
            if (OpCode->RightValue)
            {
                printf("bc0t 0x%04lX", OpCode->Immediate);
            }
            else
            {
                printf("bc0f 0x%04lX", OpCode->Immediate);
            }
        } break;
        case 0b10000:
            printf("cop0 0x%08lX", OpCode->Immediate);
            break;
    }
}

static void
COP1(disasm_opcode_info *OpCode)
{
    switch (OpCode->FunctionSelect) {
        case 0b00000:
            printf("mfc1 %s, %s", RNT[OpCode->DestinationRegister], CNName(OpCode->LeftValue));
            break;
        case 0b00010:
            printf("cfc1 %s, %s", RNT[OpCode->DestinationRegister], CNName(OpCode->LeftValue - 32));
            break;
        case 0b00100:
            printf("mtc1 %s, %s", RNT[OpCode->LeftValue], CNName(OpCode->DestinationRegister));
            break;
        case 0b00110:
            printf("ctc1 %s, %s", RNT[OpCode->LeftValue], CNName(OpCode->DestinationRegister - 32));
            break;
        case 0b01000:
        {
            if (OpCode->RightValue)
            {
                printf("bc1t 0x%04lX", OpCode->Immediate);
            }
            else
            {
                printf("bc1f 0x%04lX", OpCode->Immediate);
            }
        } break;
        case 0b10000:
            printf("cop1 0x%08lX", OpCode->Immediate);
            break;
    }
}

static void
COP2(disasm_opcode_info *OpCode)
{
    switch (OpCode->FunctionSelect) {
        case 0b00000:
            printf("mfc2 %s, %s", RNT[OpCode->DestinationRegister], CNName(OpCode->LeftValue));
            break;
        case 0b00010:
            printf("cfc2 %s, %s", RNT[OpCode->DestinationRegister], CNName(OpCode->LeftValue - 32));
            break;
        case 0b00100:
            printf("mtc2 %s, %s", RNT[OpCode->LeftValue], CNName(OpCode->DestinationRegister));
            break;
        case 0b00110:
            printf("ctc2 %s, %s", RNT[OpCode->LeftValue], CNName(OpCode->DestinationRegister - 32));
            break;
        case 0b01000:
        {
            if (OpCode->RightValue)
            {
                printf("bc2t 0x%04lX", OpCode->Immediate);
            }
            else
            {
                printf("bc2f 0x%04lX", OpCode->Immediate);
            }
        } break;
        case 0b10000:
            printf("cop2 0x%08lX", OpCode->Immediate);
            break;
    }
}

static void
COP3(disasm_opcode_info *OpCode)
{
    switch (OpCode->FunctionSelect) {
        case 0b00000:
            printf("mfc3 %s, %s", RNT[OpCode->DestinationRegister], CNName(OpCode->LeftValue));
            break;
        case 0b00010:
            printf("cfc3 %s, %s", RNT[OpCode->DestinationRegister], CNName(OpCode->LeftValue - 32));
            break;
        case 0b00100:
            printf("mtc3 %s, %s", RNT[OpCode->LeftValue], CNName(OpCode->DestinationRegister));
            break;
        case 0b00110:
            printf("ctc3 %s, %s", RNT[OpCode->LeftValue], CNName(OpCode->DestinationRegister - 32));
            break;
        case 0b01000:
        {
            if (OpCode->RightValue)
            {
                printf("bc3t 0x%04lX", OpCode->Immediate);
            }
            else
            {
                printf("bc3f 0x%04lX", OpCode->Immediate);
            }
        } break;
        case 0b10000:
            printf("cop3 0x%08lX", OpCode->Immediate);
            break;
    }
}


void
DisassemblerDecodeOpcode(disasm_opcode_info *OpCode, u32 Data, u32 IAddress)
{
    OpCode->CurrentAddress = IAddress;
    OpCode->Select0 = (Data & PRIMARY_OP_MASK) >> 26;
    OpCode->Select1 = (Data & SECONDARY_OP_MASK) >> 0;
    OpCode->MemAccessType = MEM_ACCESS_NONE;
    OpCode->MemAccessMode = MEM_ACCESS_WORD;
    OpCode->WriteBackMode = WRITE_BACK_CPU;
    OpCode->LeftValue = 0;
    OpCode->RightValue = 0;
    OpCode->Immediate = 0;
    OpCode->Result = 0;
    OpCode->DestinationRegister = 0;
    OpCode->RADestinationRegister = 0;
    OpCode->FunctionSelect = 0;
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (OpCode->Select0 == 0)
    {
        //shift-imm
        if ((OpCode->Select1 & 0b111100) == 0)
        {
            OpCode->Immediate = (Data & IMM5_MASK) >> 6;
            OpCode->RightValue = rt;
            OpCode->DestinationRegister = rd;
        }
        //shift-reg
        else if ((OpCode->Select1 & 0b111000) == 0)
        {
            OpCode->LeftValue = rs;
            OpCode->RightValue = rt;
            OpCode->DestinationRegister = rd;
        }
        //jr
        else if (OpCode->Select1 == 0b001000)
        {
            OpCode->LeftValue = rs;
            OpCode->DestinationRegister = REG_INDEX_PC;
//            OpCode->MemAccessType = MEM_ACCESS_BRANCH;
        }
        //jalr
        else if (OpCode->Select1 == 0b001001)
        {
            OpCode->LeftValue = rs;
            OpCode->DestinationRegister = REG_INDEX_PC;
            OpCode->RADestinationRegister = rd;
//            OpCode->MemAccessType = MEM_ACCESS_BRANCH;
        }
        //sys/brk
        else if ((OpCode->Select1 & 0b111110) == 0b001100)
        {
            OpCode->Immediate = (Data & COMMENT20_MASK) >> 6;
        }
        //mfhi/mflo
        else if ((OpCode->Select1 & 0b111101) == 0b010000)
        {
            OpCode->DestinationRegister = rd;
        }
        //mthi/mtlo
        else if ((OpCode->Select1 & 0b111101) == 0b010001)
        {
            OpCode->LeftValue = rs;
        }
        //mul/div
        else if ((OpCode->Select1 & 0b111100) == 0b011000)
        {
            OpCode->LeftValue = rs;
            OpCode->RightValue = rt;
        }
        //alu-reg
        else if (OpCode->Select1 & 0b100000)
        {
            OpCode->LeftValue = rs;
            OpCode->RightValue = rt;
            OpCode->DestinationRegister = rd;
        }

    }
    //bltz, bgez, bltzal bgezal
    else if (OpCode->Select0 == 0b000001)
    {
        OpCode->LeftValue = rs;
        OpCode->FunctionSelect = rt;
        OpCode->Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
        //destination registers set within function
    }
    //j/jal
    else if ((OpCode->Select0 & 0b111110) == 0b000010)
    {
        OpCode->Immediate = (Data & IMM26_MASK) >> 0;
        OpCode->DestinationRegister = REG_INDEX_PC;
//        OpCode->MemAccessType = MEM_ACCESS_BRANCH;
        //RADestinationRegister set within function
    }
    //beq/bne
    else if ((OpCode->Select0 & 0b111110) == 0b000100)
    {
        OpCode->LeftValue = rs;
        OpCode->RightValue = rt;
        OpCode->Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
        //destination registers set within function
    }
    //blez/bgtz
    else if ((OpCode->Select0 & 0b111110) == 0b000110)
    {
        OpCode->LeftValue = rs;
        OpCode->Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
        //destination registers set within function
    }
    //alu-imm
    else if ((OpCode->Select0 & 0b111000) == 0b001000)
    {
        OpCode->LeftValue = rs;
        OpCode->Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
        OpCode->DestinationRegister = rt;
    }
    //lui-imm
    else if (OpCode->Select0 == 0b001111)
    {
        OpCode->Immediate = (Data & IMM16_MASK) >> 0;
        OpCode->DestinationRegister = rt;
    }
    //load
    else if ((OpCode->Select0 & 0b111000) == 0b100000)
    {
        OpCode->LeftValue = rs;
        OpCode->Immediate = (Data & IMM16_MASK) >> 0;
        OpCode->DestinationRegister = rt;
        OpCode->MemAccessType = MEM_ACCESS_READ;
    }
    //store
    else if ((OpCode->Select0 & 0b111000) == 0b101000)
    {
        OpCode->LeftValue = rs;
        OpCode->RightValue = rt;
        OpCode->Immediate = (Data & IMM16_MASK) >> 0;
        OpCode->MemAccessType = MEM_ACCESS_WRITE;
    }
    // coprocessor main instruction decoding
    else if ((OpCode->Select0 & 0b010000) == 0b010000)
    {
        OpCode->FunctionSelect = rs;
        // mfc, cfc
        if (rs < 0b00100)
        {
            OpCode->DestinationRegister = rt;
            OpCode->LeftValue = rd + (rs & 0b00010 ? 32 : 0);
        }
        // mtc, ctc
        else if (rs < 0b01000)
        {
            OpCode->WriteBackMode = OpCode->Select0 & 0b111;
            OpCode->DestinationRegister = rd + (rs & 0b00010 ? 32 : 0);
            OpCode->LeftValue = rt;
        }
        // BCnF, BCnT
        else if (rs == 0b01000)
        {
            OpCode->RightValue = rt; // used as secondary function select
            OpCode->Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
        }
        else if (rs & 0b10000)
        {
            OpCode->Immediate = (Data & IMM25_MASK) >> 0;
        }
    }
    //lwc
    else if ((OpCode->Select0 & 0b111000) == 0b110000)
    {
        OpCode->WriteBackMode = OpCode->Select0 & 0b111;
        OpCode->LeftValue = rs;
        OpCode->DestinationRegister = rt;
        OpCode->Immediate = (Data & IMM16_MASK) >> 0;
        OpCode->MemAccessType = MEM_ACCESS_READ;
    }
    //swc
    else if ((OpCode->Select0 & 0b111000) == 0b111000)
    {
        OpCode->LeftValue = rs;
        OpCode->RightValue = rt;
        OpCode->Immediate = (Data & IMM16_MASK) >> 0;
        OpCode->MemAccessType = MEM_ACCESS_WRITE;
    }
}

static void
PrintSecondary(disasm_opcode_info *Op)
{
    SecondaryJumpTable[Op->Select1](Op);
}

void
DisassemblerPrintOpCode(disasm_opcode_info *OpCode)
{
    printf("0x%08lX: ", OpCode->CurrentAddress);
    PrimaryJumpTable[OpCode->Select0](OpCode);
    printf("\n");
}

void
DisassemblerPrintRange(MIPS_R3000 *Cpu, u32 Base, u32 Count, u32 PC)
{
    for (u32 i = 0; i < Count; ++i)
    {
        u32 MachineCode = ReadMemWordRaw(Cpu, Base + (i * 4));
        disasm_opcode_info OpCode;
        printf("\x1b[0m");
        if (Base + i * 4 == PC)
        {
            printf("\x1b[32m"); // IF
        }
        if (Base + i * 4 == PC - 4)
        {
            printf("\x1b[33m"); // DC
        }
        if (Base + i * 4 == PC - 8)
        {
            printf("\x1b[31m"); // EXE
        }
        if (Base + i * 4 == PC - 12)
        {
            printf("\x1b[34m"); // MEM
        }
        if (Base + i * 4 == PC - 16)
        {
            printf("\x1b[36m"); // WB
        }
        DisassemblerDecodeOpcode(&OpCode, MachineCode, Base + i * 4);
        DisassemblerPrintOpCode(&OpCode);
    }
}

static void __attribute__((constructor))
InitJumpTables()
{
    for (int i = 0; i < 0x3F; ++i)
    {
        PrimaryJumpTable[i] = UnknownOpCode;
        SecondaryJumpTable[i] = UnknownOpCode;
    }

    PrimaryJumpTable[0x00] = PrintSecondary;
    PrimaryJumpTable[0x01] = BranchZero;
    PrimaryJumpTable[0x02] = J;
    PrimaryJumpTable[0x03] = JAL;
    PrimaryJumpTable[0x04] = BEQ;
    PrimaryJumpTable[0x05] = BNE;
    PrimaryJumpTable[0x06] = BLEZ;
    PrimaryJumpTable[0x07] = BGTZ;
    PrimaryJumpTable[0x08] = AddI;
    PrimaryJumpTable[0x09] = AddIU;
    PrimaryJumpTable[0x0A] = SLTI;
    PrimaryJumpTable[0x0B] = SLTIU;
    PrimaryJumpTable[0x0C] = AndI;
    PrimaryJumpTable[0x0D] = OrI;
    PrimaryJumpTable[0x0E] = XOrI;
    PrimaryJumpTable[0x0F] = LUI;
    PrimaryJumpTable[0x10] = COP0;
    PrimaryJumpTable[0x11] = COP1;
    PrimaryJumpTable[0x12] = COP2;
    PrimaryJumpTable[0x13] = COP3;
    PrimaryJumpTable[0x20] = LB;
    PrimaryJumpTable[0x21] = LH;
    //    PrimaryJumpTable[0x22] = LWL;
    PrimaryJumpTable[0x23] = LW;
    PrimaryJumpTable[0x24] = LBU;
    PrimaryJumpTable[0x25] = LHU;
    //    PrimaryJumpTable[0x26] = LWR;
    PrimaryJumpTable[0x28] = SB;
    PrimaryJumpTable[0x29] = SH;
    //    PrimaryJumpTable[0x2A] = SWL;
    PrimaryJumpTable[0x2B] = SW;
    //    PrimaryJumpTable[0x2E] = SWR;
    //    PrimaryJumpTable[0x30] = LWC0;
    //    PrimaryJumpTable[0x31] = LWC1;
    //    PrimaryJumpTable[0x32] = LWC2;
    //    PrimaryJumpTable[0x33] = LWC3;
    //    PrimaryJumpTable[0x38] = SWC0;
    //    PrimaryJumpTable[0x39] = SWC1;
    //    PrimaryJumpTable[0x3A] = SWC2;
    //    PrimaryJumpTable[0x3B] = SWC3;

    SecondaryJumpTable[0x00] = SLL;
    SecondaryJumpTable[0x02] = SRL;
    SecondaryJumpTable[0x03] = SRA;
    SecondaryJumpTable[0x04] = SLLV;
    SecondaryJumpTable[0x06] = SRLV;
    SecondaryJumpTable[0x07] = SRAV;
    SecondaryJumpTable[0x08] = JR;
    SecondaryJumpTable[0x09] = JALR;
    SecondaryJumpTable[0x0C] = SysCall;
    SecondaryJumpTable[0x0D] = Break;
    SecondaryJumpTable[0x10] = MFHI;
    SecondaryJumpTable[0x11] = MTHI;
    SecondaryJumpTable[0x12] = MFLO;
    SecondaryJumpTable[0x13] = MTLO;
    SecondaryJumpTable[0x18] = Mult;
    SecondaryJumpTable[0x19] = MultU;
    SecondaryJumpTable[0x1A] = Div;
    SecondaryJumpTable[0x1B] = DivU;
    SecondaryJumpTable[0x20] = Add;
    SecondaryJumpTable[0x21] = AddU;
    SecondaryJumpTable[0x22] = Sub;
    SecondaryJumpTable[0x23] = SubU;
    SecondaryJumpTable[0x24] = And;
    SecondaryJumpTable[0x25] = Or;
    SecondaryJumpTable[0x26] = XOr;
    SecondaryJumpTable[0x27] = NOr;
    SecondaryJumpTable[0x2A] = SLT;
    SecondaryJumpTable[0x2B] = SLTU;
}
