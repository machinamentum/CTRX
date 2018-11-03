/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 94):
 * <joshuahuelsman@gmail.com> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Josh Huelsman
 * ----------------------------------------------------------------------------
 */
#include <stdio.h>
#include "mips.h"



inline u32
ReadMemWord(MIPS_R3000 *Cpu, u32 Address)
{
    u32 Base = Address & 0x00FFFFFF;
    u8 *VirtualAddress = (u8 *)MapVirtualAddress(Cpu, Base);
    u32 Swap = Cpu->CP0.sr & C0_STATUS_RE;
    u32 Value = -1;
    if (!VirtualAddress)
    {
        for (u32 i = 0; i < Cpu->NumMMR; ++i)
        {
            mmr *MMR = &Cpu->MemMappedRegisters[i];
            if (Base == MMR->Address)
            {
                Value = MMR->RegisterReadFunc(MMR->Object, Address);
                return (Swap ? __builtin_bswap32(Value) : Value);
            }
        }

        return Value;
    }
    Value = *((u32 *)VirtualAddress);
    return (Swap ? __builtin_bswap32(Value) : Value);
}

static void
WriteMemByte(MIPS_R3000 *Cpu, u32 Address, u8 value)
{
    u32 Base = Address & 0x00FFFFFF;
    u8 *VirtualAddress = (u8 *)MapVirtualAddress(Cpu, Base);
    if (!VirtualAddress)
    {
        for (u32 i = 0; i < Cpu->NumMMR; ++i)
        {
            mmr *MMR = &Cpu->MemMappedRegisters[i];
            if (Base == MMR->Address)
            {
                
                MMR->RegisterWriteFunc(MMR->Object, value);
                return;
            }
        }

        return;
    }
    *((u8 *)VirtualAddress) = value;
}

static void
WriteMemWord(MIPS_R3000 *Cpu, u32 Address, u32 Value)
{
    u32 Base = Address & 0x00FFFFFF;
    u8 *VirtualAddress = (u8 *)MapVirtualAddress(Cpu, Base);
    if (!VirtualAddress)
    {
        for (u32 i = 0; i < Cpu->NumMMR; ++i)
        {
            mmr *MMR = &Cpu->MemMappedRegisters[i];
            if (Base == MMR->Address)
            {
                MMR->RegisterWriteFunc(MMR->Object, Value);
                return;
            }
        }

        return;
    }
    u32 Swap = Cpu->CP0.sr & C0_STATUS_RE;
    *((u32 *)((u8 *)VirtualAddress)) = (Swap ? __builtin_bswap32(Value) : Value);
}

static void
WriteMemHalfWord(MIPS_R3000 *Cpu, u32 Address, u16 Value)
{
    u32 Base = Address & 0x00FFFFFF;
    u8 *VirtualAddress = (u8 *)MapVirtualAddress(Cpu, Base);
    if (!VirtualAddress)
    {
        for (u32 i = 0; i < Cpu->NumMMR; ++i)
        {
            mmr *MMR = &Cpu->MemMappedRegisters[i];
            if (Base == MMR->Address)
            {
                MMR->RegisterWriteFunc(MMR->Object, Value);
                return;
            }
        }

        return;
    }
    u32 Swap = Cpu->CP0.sr & C0_STATUS_RE;
    *((u16 *)((u8 *)VirtualAddress)) = (Swap ? __builtin_bswap16(Value) : Value);
}

static void
C0ExecuteOperation(Coprocessor *Cp, u32 FunctionCode);

MIPS_R3000::
MIPS_R3000()
{
    CP0.ExecuteOperation = C0ExecuteOperation;
    RAM = linearAlloc(2048 * 1000);
    BIOS = linearAlloc(512 * 1000);
    NumMMR = 0;
}

//Exceptions
inline void
C0ExceptionPushSRBits(Coprocessor *CP0)
{
    u32 SR = CP0->sr;
    u32 IEp = (SR >> 2) & 1;
    u32 KUp = (SR >> 3) & 1;
    u32 IEc = (SR) & 1;
    u32 KUc = (SR >> 1) & 1;
    SR ^= (-IEp ^ SR) & (1 << 4);
    SR ^= (-KUp ^ SR) & (1 << 5);
    SR ^= (-IEc ^ SR) & (1 << 2);
    SR ^= (-KUc ^ SR) & (1 << 3);
    CP0->sr = SR;
}

inline void
C0ExceptionPopSRBits(Coprocessor *CP0)
{
    u32 SR = CP0->sr;
    u32 IEp = (SR >> 2) & 1;
    u32 KUp = (SR >> 3) & 1;
    u32 IEo = (SR >> 4) & 1;
    u32 KUo = (SR >> 5) & 1;
    SR ^= (-IEp ^ SR) & (1 << 0);
    SR ^= (-KUp ^ SR) & (1 << 1);
    SR ^= (-IEo ^ SR) & (1 << 2);
    SR ^= (-KUo ^ SR) & (1 << 3);
    CP0->sr = SR;
}

void
C0GenerateException(MIPS_R3000 *Cpu, u8 Cause, u32 EPC)
{
    if (Cpu->CP0.sr & C0_STATUS_IEc)
    {
        Cpu->CP0.cause = (Cause << 2) & C0_CAUSE_MASK;
        Cpu->CP0.epc = EPC;
        C0ExceptionPushSRBits(&Cpu->CP0);
        Cpu->CP0.sr &= ~C0_STATUS_KUc;
        Cpu->CP0.sr &= ~C0_STATUS_IEc;
        Cpu->pc = GNRAL_VECTOR;
    }
}

static void
C0GenerateSoftwareException(MIPS_R3000 *Cpu, u8 Cause, u32 EPC)
{
    Cpu->CP0.cause = (Cause << 2) & C0_CAUSE_MASK;
    Cpu->CP0.epc = EPC;
    C0ExceptionPushSRBits(&Cpu->CP0);
    Cpu->CP0.sr &= ~C0_STATUS_KUc;
    Cpu->CP0.sr &= ~C0_STATUS_IEc;
    Cpu->pc = GNRAL_VECTOR;
}

static void
C0ReturnFromException(Coprocessor *Cp)
{
    C0ExceptionPopSRBits(Cp);
}

static void
C0ExecuteOperation(Coprocessor *Cp, u32 FunctionCode)
{
    if (FunctionCode == 0x10)
    {
        C0ReturnFromException(Cp);
    }
}

static void
ReservedInstructionException(MIPS_R3000 *Cpu, opcode *Op, u32 Data)
{
    C0GenerateException(Cpu, C0_CAUSE_RI, Op->CurrentAddress);
}

static void
SysCall(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
//    u32 Immediate = (Data & COMMENT20_MASK) >> 6;
    C0GenerateSoftwareException(Cpu, C0_CAUSE_SYSCALL, OpCode->CurrentAddress);
}

static void
Break(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
//    OpCode->Immediate = (Data & COMMENT20_MASK) >> 6;
    C0GenerateException(Cpu, C0_CAUSE_BKPT, OpCode->CurrentAddress);
}

typedef void (*jt_func)(MIPS_R3000 *, opcode *, u32 Data);

//Arithmetic
static void
AddU(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rs] + Cpu->registers[rt];
    }
}

static void
AddIU(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;

    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
        Cpu->registers[rt] = Cpu->registers[rs] + Immediate;
    }
}

static void
SubU(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rs] - Cpu->registers[rt];
    }
}

static void
Add(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rs] + Cpu->registers[rt];
    }
    // TODO overflow trap
}

static void
AddI(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;

    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
        Cpu->registers[rt] = Cpu->registers[rs] + Immediate;
    }
    // TODO overflow trap
}

static void
Sub(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rs] - Cpu->registers[rt];
    }
    // TODO overflow trap
}

//HI:LO operations
static void
MFHI(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;
    Cpu->registers[rd] = Cpu->hi;
}

static void
MFLO(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;
    Cpu->registers[rd] = Cpu->lo;
}

static void
MTHI(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    Cpu->hi = rs;
}

static void
MTLO(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    Cpu->lo = rs;
}

static void
Mult(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;

    s64 Result = (s64)Cpu->registers[rs] * (s64)Cpu->registers[rt];
    Cpu->hi = (Result >> 32) & 0xFFFFFFFF;
    Cpu->lo = Result & 0xFFFFFFFF;
}

static void
MultU(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;

    u64 Result = (u64)Cpu->registers[rs] * (u64)Cpu->registers[rt];
    Cpu->hi = Result >> 32;
    Cpu->lo = Result & 0xFFFFFFFF;
}

static void
Div(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;

    s32 Left = Cpu->registers[rs];
    s32 Right = Cpu->registers[rt];
    if (!Right)
    {
        Cpu->hi = Left;
        if (Left >= 0)
        {
            Cpu->lo = -1;
        }
        else
        {
            Cpu->lo = 1;
        }
        return;
    }
    if (Right == -1 && (u32)Left == 0x80000000)
    {
        Cpu->hi = 0;
        Cpu->lo = 0x80000000;
        return;
    }
    Cpu->lo = Left / Right;
    Cpu->hi = Left % Right;
}

static void
DivU(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;

    u32 Left = Cpu->registers[rs];
    u32 Right = Cpu->registers[rt];
    if (!Right)
    {
        Cpu->hi = Left;
        Cpu->lo = 0xFFFFFFFF;
        return;
    }
    Cpu->lo = Left / Right;
    Cpu->hi = Left % Right;
}

//Store
static void
SW(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u32 Immediate = SignExtend16((Data & IMM16_MASK));
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;

    OpCode->MemAccessAddress = Cpu->registers[rs] + Immediate;
    OpCode->MemAccessValue = Cpu->registers[rt];
    OpCode->MemAccessMode = MEM_ACCESS_WORD | MEM_ACCESS_WRITE;
}

static void
SH(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u32 Immediate = SignExtend16((Data & IMM16_MASK));
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;

    OpCode->MemAccessAddress = Cpu->registers[rs] + Immediate;
    OpCode->MemAccessValue = Cpu->registers[rt];
    OpCode->MemAccessMode = MEM_ACCESS_HALF | MEM_ACCESS_WRITE;
}

static void
SB(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u32 Immediate = SignExtend16((Data & IMM16_MASK));
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;

    OpCode->MemAccessAddress = Cpu->registers[rs] + Immediate;
    OpCode->MemAccessValue = Cpu->registers[rt];
    OpCode->MemAccessMode = MEM_ACCESS_BYTE | MEM_ACCESS_WRITE;
}

//Load
static void
LUI(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;
    if (rt)
    {
        u32 Immediate = (Data & IMM16_MASK) >> 0;
        Cpu->registers[rt] = Immediate << 16;
    }
}

static void
LW(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;
    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = SignExtend16((Data & IMM16_MASK));
        OpCode->MemAccessAddress = Cpu->registers[rs] + Immediate;
        OpCode->MemAccessValue = rt;
        OpCode->MemAccessMode = MEM_ACCESS_READ | MEM_ACCESS_WORD;
    }
}

static void
LBU(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;
    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = SignExtend16((Data & IMM16_MASK));
        OpCode->MemAccessAddress = Cpu->registers[rs] + Immediate;
        OpCode->MemAccessValue = rt;
        OpCode->MemAccessMode = MEM_ACCESS_READ | MEM_ACCESS_BYTE;
    }
}

static void
LHU(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;
    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = SignExtend16((Data & IMM16_MASK));
        OpCode->MemAccessAddress = Cpu->registers[rs] + Immediate;
        OpCode->MemAccessValue = rt;
        OpCode->MemAccessMode = MEM_ACCESS_READ | MEM_ACCESS_HALF;
    }
}

static void
LB(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;
    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = SignExtend16((Data & IMM16_MASK));
        OpCode->MemAccessAddress = Cpu->registers[rs] + Immediate;
        OpCode->MemAccessValue = rt;
        OpCode->MemAccessMode = MEM_ACCESS_READ | MEM_ACCESS_BYTE | MEM_ACCESS_SIGNED;
    }
}

static void
LH(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;
    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = SignExtend16((Data & IMM16_MASK));
        OpCode->MemAccessAddress = Cpu->registers[rs] + Immediate;
        OpCode->MemAccessValue = rt;
        OpCode->MemAccessMode = MEM_ACCESS_READ | MEM_ACCESS_HALF | MEM_ACCESS_SIGNED;
    }
}


// Jump/Call
static void
J(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u32 Immediate = (Data & IMM26_MASK) >> 0;
    Cpu->pc = (Cpu->pc & 0xF0000000) + (Immediate * 4);
}

static void
JAL(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u32 Immediate = (Data & IMM26_MASK) >> 0;
    Cpu->ra = OpCode->CurrentAddress + 8;
    Cpu->pc = (Cpu->pc & 0xF0000000) + (Immediate * 4);
}

static void
JR(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    Cpu->pc = Cpu->registers[rs];
}

static void
JALR(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rd = (Data & REG_RD_MASK) >> 11;
    Cpu->pc = Cpu->registers[rs];
    if (rd) Cpu->registers[rd] = OpCode->CurrentAddress + 8;
}

static void
BranchZero(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;

    u32 Immediate = SignExtend16((Data & IMM16_MASK) >> 0);

    //bltz, bgez, bltzal, bgezal
    u32 Address = OpCode->CurrentAddress + 4 + Immediate * 4;
    s32 Check = Cpu->registers[rs];

    if (rt & 0b00001)
    {
        //bgez
        if (Check >= 0)
        {
            if (rt & 0b10000)
            {
                Cpu->ra = OpCode->CurrentAddress + 8;
            }
            Cpu->pc = Address;
        }
    }
    else
    {
        //bltz
        if (Check < 0)
        {
            if (rt & 0b10000)
            {
                Cpu->ra = OpCode->CurrentAddress + 8;
            }
            Cpu->pc = Address;
        }
    }
}

static void
BEQ(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;
    
    if (Cpu->registers[rs] == Cpu->registers[rt])
    {
        u32 Immediate = SignExtend16((Data & IMM16_MASK));
        Cpu->pc = OpCode->CurrentAddress + 4 + Immediate * 4;
    }
}

static void
BNE(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;

    if (Cpu->registers[rs] != Cpu->registers[rt])
    {
        u32 Immediate = SignExtend16((Data & IMM16_MASK));
        Cpu->pc = OpCode->CurrentAddress + 4 + Immediate * 4;
    }
}

static void
BLEZ(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;

    if (Cpu->registers[rs] <= 0)
    {
        u32 Immediate = SignExtend16((Data & IMM16_MASK));
        Cpu->pc = OpCode->CurrentAddress + 4 + Immediate * 4;
    }
}

static void
BGTZ(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;

    if (Cpu->registers[rs] > 0)
    {
        u32 Immediate = SignExtend16((Data & IMM16_MASK));
        Cpu->pc = OpCode->CurrentAddress + 4 + Immediate * 4;
    }
}

//Logical
static void
AndI(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;

    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = Data & IMM16_MASK;
        Cpu->registers[rt] = Cpu->registers[rs] & Immediate;
    }
}

static void
OrI(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;

    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = Data & IMM16_MASK;
        Cpu->registers[rt] = Cpu->registers[rs] | Immediate;
    }
}

static void
And(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rs] & Cpu->registers[rt];
    }
}

static void
Or(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rs] | Cpu->registers[rt];
    }
}

static void
XOr(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rs] ^ Cpu->registers[rt];
    }
}
static void
NOr(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = 0xFFFFFFFF ^ (Cpu->registers[rs] | Cpu->registers[rt]);
    }
}

static void
XOrI(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;

    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = Data & IMM16_MASK;
        Cpu->registers[rt] = Cpu->registers[rs] ^ Immediate;
    }
}

//shifts
static void
SLLV(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rt] << (Cpu->registers[rs] & 0x1F);
    }
}

static void
SRLV(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rt] >> (Cpu->registers[rs] & 0x1F);
    }
}

static void
SRAV(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = ((s32)Cpu->registers[rt]) >> (Cpu->registers[rs] & 0x1F);
    }
}

static void
SLL(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 Immediate = (Data & IMM5_MASK) >> 6;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rt] << Immediate;
    }
}

static void
SRL(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 Immediate = (Data & IMM5_MASK) >> 6;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = Cpu->registers[rt] >> Immediate;
    }
}

static void
SRA(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 Immediate = (Data & IMM5_MASK) >> 6;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = ((s32)Cpu->registers[rt]) >> Immediate;
    }
}

// comparison
static void
SLT(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = ( ((s32)Cpu->registers[rs] < (s32)Cpu->registers[rt]) ? 1 : 0);
    }
}

static void
SLTU(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rd = (Data & REG_RD_MASK) >> 11;

    if (rd)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u8 rt = (Data & REG_RT_MASK) >> 16;
        Cpu->registers[rd] = ( (Cpu->registers[rs] < Cpu->registers[rt]) ? 1 : 0);
    }
}

static void
SLTI(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;

    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
        Cpu->registers[rt] = ( ((s32)Cpu->registers[rs] < (s32)Immediate) ? 1 : 0);
    }
}

static void
SLTIU(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rt = (Data & REG_RT_MASK) >> 16;

    if (rt)
    {
        u8 rs = (Data & REG_RS_MASK) >> 21;
        u32 Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
        Cpu->registers[rt] = ( (Cpu->registers[rs] < Immediate) ? 1 : 0);
    }
}

// coprocessor ops
static void
COP0(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;
    u8 rd = (Data & REG_RD_MASK) >> 11;

    Coprocessor *CP0 = &Cpu->CP0;

    if (rs < 0b001000 && rs > 0b00010)
    {
        CP0->registers[rd + (rs & 0b00010 ? 32 : 0)] = Cpu->registers[rt];
    }
    else if (rs < 0b10000)
    {
        if (rs < 0b00100)
        {
            if (rt) Cpu->registers[rt] = CP0->registers[rd + (rs & 0b00010 ? 32 : 0)];
        }
        else
        {
            u32 Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
            if (rt)
            {
                if (CP0->sr & C0_STATUS_CU0)
                {
                    Cpu->pc = OpCode->CurrentAddress + Immediate;
                }
            }
            else
            {
                if ((CP0->sr & C0_STATUS_CU0) == 0)
                {
                    Cpu->pc = OpCode->CurrentAddress + Immediate;
                }
            }
        }
    }
    else
    {
        CP0->ExecuteOperation(CP0, (Data & IMM25_MASK));
    }
}

static void
COP1(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    // PSX missing cop1
    // TODO exceptions
}

static void
COP2(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    u8 rs = (Data & REG_RS_MASK) >> 21;
    u8 rt = (Data & REG_RT_MASK) >> 16;
    u8 rd = (Data & REG_RD_MASK) >> 11;

    Coprocessor *CP2 = Cpu->CP2;
    if (rs < 0b001000 && rs > 0b00010)
    {
        CP2->registers[rd + (rs & 0b00010 ? 32 : 0)] = Cpu->registers[rt];
    }
    else if (rs < 0b10000)
    {
        if (rs < 0b00100)
        {
            if (rt) Cpu->registers[rt] = CP2->registers[rd + (rs & 0b00010 ? 32 : 0)];
        }
        else
        {
            u32 Immediate = SignExtend16((Data & IMM16_MASK) >> 0);
            if (rt)
            {
                if (Cpu->CP0.sr & C0_STATUS_CU2)
                {
                    Cpu->pc = OpCode->CurrentAddress + Immediate;
                }
            }
            else
            {
                if ((Cpu->CP0.sr & C0_STATUS_CU2) == 0)
                {
                    Cpu->pc = OpCode->CurrentAddress + Immediate;
                }
            }
        }
    }
    else
    {
        CP2->ExecuteOperation(CP2, (Data & IMM25_MASK));
    }
}

static void
COP3(MIPS_R3000 *Cpu, opcode *OpCode, u32 Data)
{
    // PSX missing cop3
    // TODO exceptions
}

inline u32
InstructionFetch(MIPS_R3000 *Cpu)
{
    u32 Result = ReadMemWord(Cpu, Cpu->pc);
    Cpu->pc += 4;
    return Result;
}

inline void
MemoryAccess(MIPS_R3000 *Cpu, opcode *OpCode)
{
    u32 Address = OpCode->MemAccessAddress;
    u32 Value = OpCode->MemAccessValue;
    u32 MemAccessMode = OpCode->MemAccessMode;

    if (MemAccessMode & MEM_ACCESS_WRITE)
    {
        if (MemAccessMode & MEM_ACCESS_BYTE)
        {
            WriteMemByte(Cpu, Address, Value);
        }

        else if (MemAccessMode & MEM_ACCESS_HALF)
        {
            WriteMemHalfWord(Cpu, Address, Value);
        }

        else if (MemAccessMode & MEM_ACCESS_WORD)
        {
            WriteMemWord(Cpu, Address, Value);
        }
    }
    else if (MemAccessMode & MEM_ACCESS_READ)
    {
        if (Value)
        {
            u32 Register = Value;
            u32 Signed = MemAccessMode & MEM_ACCESS_SIGNED;
            Value = ReadMemWord(Cpu, Address);
            if (MemAccessMode & MEM_ACCESS_BYTE)
            {
                Value &= 0xFF;
                if (Signed)
                {
                    Value = SignExtend8(Value);
                }
            }

            else if (MemAccessMode & MEM_ACCESS_HALF)
            {
                Value &= 0xFFFF;
                if (Signed)
                {
                    Value = SignExtend16(Value);
                }
            }
            Cpu->registers[Register] = Value;
        }
    }
}

void
MapRegister(MIPS_R3000 *Cpu, mmr MMR)
{
    Cpu->MemMappedRegisters[Cpu->NumMMR] = MMR;
    Cpu->MemMappedRegisters[Cpu->NumMMR].Address &= 0x00FFFFFF;
    ++Cpu->NumMMR;
}

void
StepCpu(MIPS_R3000 *Cpu, u32 Steps)
{
    void *FJTPrimary[0x40] =
    {
        &&_ReservedInstructionException,
        &&_BranchZero,
        &&_Jump,
        &&_JumpAL,
        &&_BEQ,
        &&_BNE,
        &&_BLEZ,
        &&_BGTZ,
        &&_AddI,
        &&_AddIU,
        &&_SLTI,
        &&_SLTIU,
        &&_AndI,
        &&_OrI,
        &&_XOrI,
        &&_LUI,
        &&_COP0,
        &&_COP1,
        &&_COP2,
        &&_COP3,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_LB,
        &&_LH,
        &&_ReservedInstructionException, // &&_LWL,
        &&_LW,
        &&_LBU,
        &&_LHU,
        &&_ReservedInstructionException, // &&_LWR,
        &&_ReservedInstructionException,
        &&_SB,
        &&_SH,
        &&_ReservedInstructionException, // &&_SWL,
        &&_SW,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException, // &&_SWR,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException, // &&_LWC0,
        &&_ReservedInstructionException, // &&_LWC1,
        &&_ReservedInstructionException, // &&_LWC2,
        &&_ReservedInstructionException, // &&_LWC3,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException, //&&_SWC0,
        &&_ReservedInstructionException, //&&_SWC1,
        &&_ReservedInstructionException, //&&_SWC2,
        &&_ReservedInstructionException, //&&_SWC3,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
    };

    void *FJTSecondary[0x40] =
    {
        &&_SLL,
        &&_ReservedInstructionException,
        &&_SRL,
        &&_SRA,
        &&_SLLV,
        &&_ReservedInstructionException,
        &&_SRLV,
        &&_SRAV,
        &&_JumpR,
        &&_JumpALR,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_Syscall,
        &&_Break,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_MFHI,
        &&_MTHI,
        &&_MFLO,
        &&_MTLO,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_Mult,
        &&_MultU,
        &&_Div,
        &&_DivU,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_Add,
        &&_AddU,
        &&_Sub,
        &&_SubU,
        &&_And,
        &&_Or,
        &&_XOr,
        &&_NOr,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_SLT,
        &&_SLTU,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
        &&_ReservedInstructionException,
    };

    opcode *OpCodes = Cpu->OpCodes;
    u32 BS = Cpu->BaseState;
    void *NextJump = Cpu->NextJump;
    u32 NextData = Cpu->NextData;

    if (NextJump) goto *NextJump;

#define NEXT(Instruction) \
    { \
    if (!Steps) goto _ExitThread; \
    opcode *OpCodeMemAccess = &OpCodes[BS % 2]; \
    if (OpCodeMemAccess->MemAccessMode) \
    { \
        MemoryAccess(Cpu, OpCodeMemAccess); \
    } \
    OpCodeMemAccess->CurrentAddress = Cpu->pc; \
    OpCodeMemAccess->MemAccessMode = MEM_ACCESS_NONE; \
    u32 TempData = InstructionFetch(Cpu); \
    Instruction(Cpu, &OpCodes[(BS + 1) % 2], NextData); \
    NextData = TempData; \
    if ((NextData & PRIMARY_OP_MASK) >> 26) \
    { \
        NextJump = FJTPrimary[(NextData & PRIMARY_OP_MASK) >> 26]; \
    } \
    else \
    { \
        u32 Select1 = (NextData & SECONDARY_OP_MASK); \
        NextJump = FJTSecondary[Select1]; \
    } \
    ++BS; \
    --Steps; \
    goto *NextJump; \
    }


_ReservedInstructionException:
    NEXT(ReservedInstructionException);
_BranchZero:
    NEXT(BranchZero);
_Jump:
    NEXT(J);
_JumpAL:
    NEXT(JAL);
_BEQ:
    NEXT(BEQ);
_BNE:
    NEXT(BNE);
_BLEZ:
    NEXT(BLEZ);
_BGTZ:
    NEXT(BGTZ);
_AddI:
    NEXT(AddI);
_AddIU:
    NEXT(AddIU);
_SLTI:
    NEXT(SLTI);
_SLTIU:
    NEXT(SLTIU);
_AndI:
    NEXT(AndI);
_OrI:
    NEXT(OrI);
_XOrI:
    NEXT(XOrI);
_LUI:
    NEXT(LUI);
_COP0:
    NEXT(COP0);
_COP1:
    NEXT(COP1);
_COP2:
    NEXT(COP2);
_COP3:
    NEXT(COP3);
_LB:
    NEXT(LB);
_LH:
    NEXT(LH);
_LW:
    NEXT(LW);
_LBU:
    NEXT(LBU);
_LHU:
    NEXT(LHU);
_SB:
    NEXT(SB);
_SH:
    NEXT(SH);
_SW:
    NEXT(SW);

_SLL:
    NEXT(SLL);
_SRL:
    NEXT(SRL);
_SRA:
    NEXT(SRA);
_SLLV:
    NEXT(SLLV);
_SRLV:
    NEXT(SRLV);
_SRAV:
    NEXT(SRAV);
_JumpR:
    NEXT(JR);
_JumpALR:
    NEXT(JALR);
_Syscall:
    NEXT(SysCall);
_Break:
    NEXT(Break);
_MFHI:
    NEXT(MFHI);
_MTHI:
    NEXT(MTHI);
_MFLO:
    NEXT(MFLO);
_MTLO:
    NEXT(MTLO);
_Mult:
    NEXT(Mult);
_MultU:
    NEXT(MultU);
_Div:
    NEXT(Div);
_DivU:
    NEXT(DivU);
_Add:
    NEXT(Add);
_AddU:
    NEXT(AddU);
_Sub:
    NEXT(Sub);
_SubU:
    NEXT(SubU);
_And:
    NEXT(And);
_Or:
    NEXT(Or);
_XOr:
    NEXT(XOr);
_NOr:
    NEXT(NOr);
_SLT:
    NEXT(SLT);
_SLTU:
    NEXT(SLTU);

_ExitThread:
    Cpu->NextJump = NextJump;
    Cpu->NextData = NextData;
    Cpu->BaseState = BS;
}
