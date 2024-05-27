// x64 Instruction

#pragma once

#include <stdint.h>  // int64_t

typedef struct Expr Expr;
typedef struct Name Name;

// Must match the order with kOpTable in parse_x64.c
enum Opcode {
  NOOP,
  MOV,
  MOVB,
  MOVW,
  MOVL,
  MOVQ,
  MOVSX,
  MOVZX,
  LEA,

  ADD,
  ADDQ,
  SUB,
  SUBQ,
  MUL,
  DIV,
  IDIV,
  NEG,
  NOT,
  INC,
  INCB,
  INCW,
  INCL,
  INCQ,
  DEC,
  DECB,
  DECW,
  DECL,
  DECQ,
  AND,
  OR,
  XOR,
  SHL,
  SHR,
  SAR,
  CMP,
  TEST,
  CWTL,
  CLTD,
  CQTO,

  SETO,
  SETNO,
  SETB,
  SETAE,
  SETE,
  SETNE,
  SETBE,
  SETA,
  SETS,
  SETNS,
  SETP,
  SETNP,
  SETL,
  SETGE,
  SETLE,
  SETG,

  JMP,
  JO,
  JNO,
  JB,
  JAE,
  JE,
  JNE,
  JBE,
  JA,
  JS,
  JNS,
  JP,
  JNP,
  JL,
  JGE,
  JLE,
  JG,
  CALL,
  RET,
  PUSH,
  POP,

  INT,
  SYSCALL,

  MOVSD,
  ADDSD,
  SUBSD,
  MULSD,
  DIVSD,
  XORPD,
  UCOMISD,
  CVTSI2SD,
  CVTTSD2SI,
  SQRTSD,

  MOVSS,
  ADDSS,
  SUBSS,
  MULSS,
  DIVSS,
  XORPS,
  UCOMISS,
  CVTSI2SS,
  CVTTSS2SI,

  CVTSD2SS,
  CVTSS2SD,
};

enum RegType {
  NOREG = -1,

  // 8bit
  AL,  CL,  DL,  BL,
  // 8bit (high)
  AH,  CH,  DH,  BH,

  // 8bit
  R8B,   R9B,   R10B,  R11B,
  R12B,  R13B,  R14B,  R15B,

  // 8bit: corresponds to AH~ in lower 4bit to handle easily.
  SPL = R15B + 1 + 4,
  BPL,  SIL,  DIL,

  // 16bit
  AX,   CX,   DX,   BX,   SP,   BP,   SI,   DI,
  R8W,  R9W,  R10W, R11W, R12W, R13W, R14W, R15W,

  // 32bit
  EAX,  ECX,  EDX,  EBX,  ESP,  EBP,  ESI,  EDI,
  R8D,  R9D,  R10D, R11D, R12D, R13D, R14D, R15D,

  // 64bit
  RAX,  RCX,  RDX,  RBX,  RSP,  RBP,  RSI,  RDI,
  R8,   R9,   R10,  R11,  R12,  R13,  R14,  R15,
  RIP,

  // Segment register
  CS,  DS,  ES,  FS,  GS,  SS,
};

enum RegXmmType {
  NOREGXMM = -1,
  XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
  XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,
};

enum RegSize {
  REG8,
  REG16,
  REG32,
  REG64,
};

typedef struct {
  char size;  // RegSize
  char no;  // 0~7, or RIP
  char x;   // 0 or 1, (or 2 if size == REG8, SPL~DIL)
} Reg;

enum OperandType {
  NOOPERAND,
  REG,        // %rax
  INDIRECT,   // ofs(%rax)
  INDIRECT_WITH_INDEX,   // ofs(%rax, %rcx, 4)
  IMMEDIATE,  // $1234
  DIRECT,     // foobar
  DEREF_REG,  // *%rax
  DEREF_INDIRECT,  // *ofs(%rax)
  DEREF_INDIRECT_WITH_INDEX,  // *(%rax, %rcx, 4)
  REG_XMM,
  SEGMENT_OFFSET,
};

typedef struct {
  enum OperandType type;
  union {
    Reg reg;
    int64_t immediate;
    struct {
      Expr *expr;
    } direct;
    struct {
      Expr *offset;
      Reg reg;
    } indirect;
    struct {
      Expr *offset;
      Expr *scale;
      Reg base_reg;
      Reg index_reg;
    } indirect_with_index;
    enum RegXmmType regxmm;
    struct {
      enum RegType reg;
      Expr *offset;
    } segment;
  };
} Operand;

typedef struct Inst {
  enum Opcode op;
  Operand opr[2];  // src, dst
} Inst;
