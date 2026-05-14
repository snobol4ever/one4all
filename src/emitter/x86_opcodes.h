/* x86_opcodes.h — named constants for every raw x86-64 opcode byte used in emit_core.c.
 * Multi-byte sequences: lead byte has its own name; extension/ModRM bytes named descriptively.
 * One space around * in all pointer/comment contexts per project style rules. */
#ifndef X86_OPCODES_H
#define X86_OPCODES_H
/*--- REX prefixes ---------------------------------------------------------*/
#define REX_W            0x48   /* REX.W  — 64-bit operand size, no reg extension */
#define REX_WR           0x4C   /* REX.WR — 64-bit + reg field extension          */
#define REX_B            0x41   /* REX.B  — extends r/m or base field (r8–r15)    */
#define REX_WB           0x49   /* REX.WB — 64-bit + r/m extension                */
/*--- MOV immediate → register (B8+r family) ------------------------------*/
#define MOV_EAX_IMM32    0xB8   /* mov eax, imm32  (REX_W prefix → rax, imm64)   */
#define MOV_ECX_IMM32    0xB9   /* mov ecx, imm32  (REX_W → rcx)                 */
#define MOV_EDX_IMM32    0xBA   /* mov edx, imm32  (REX_W → rdx)                 */
#define MOV_ESI_IMM32    0xBE   /* mov esi, imm32  (REX_W → rsi)                 */
#define MOV_EDI_IMM32    0xBF   /* mov edi, imm32  (REX_W → rdi)                 */
/*--- MOV reg/mem (89/8B) + common ModRM bytes ----------------------------*/
#define MOV_RM_R         0x89   /* mov r/m, r  (direction bit 0)                  */
#define MOV_R_RM         0x8B   /* mov r, r/m  (direction bit 1)                  */
#define MODRM_ECX_EAX    0xC1   /* ModRM: dst=ecx src=eax  (mod=11 reg=1 rm=0)   */
#define MODRM_EAX_ECX    0xC8   /* ModRM: cmp/mov eax,ecx  (mod=11 reg=1 rm=0)   */
#define MODRM_RDI_RAX    0xC7   /* ModRM: dst=rdi src=rax                         */
#define MODRM_RBP_RSP    0xE5   /* ModRM: dst=rbp src=rsp                         */
#define MODRM_RSP_RBP    0xEC   /* ModRM: dst=rsp src=rbp                         */
#define MODRM_R10_INDIR  0x02   /* ModRM: [r10] indirect  (mod=00 rm=010)         */
#define MODRM_RCX_INDIR  0x01   /* ModRM: [rcx] indirect  (mod=00 rm=001)         */
#define MODRM_R10D_EAX   0x02   /* ModRM: r10d/[r10] ↔ eax                        */
/*--- LEA -----------------------------------------------------------------*/
#define LEA              0x8D   /* lea r, m                                        */
#define MODRM_RAX_RAXRCX 0x04   /* ModRM SIB for lea rax,[rax+rcx]               */
#define SIB_RAX_RCX      0x08   /* SIB: scale=1 index=rcx base=rax               */
/*--- MOVZX / MOVSXD ------------------------------------------------------*/
#define ESC              0x0F   /* Two-byte escape prefix                          */
#define MOVZX_R_RM8      0xB6   /* (0F B6) movzx r32, r/m8                        */
#define MODRM_EAX_EDI7   0x47   /* ModRM: eax ← [rdi+disp8]  (mod=01 reg=0 rm=7) */
#define MOVSXD_R_RM      0x63   /* (REX_WB 63) movsxd r64, r/m32                 */
#define MODRM_RCX_R10    0x0A   /* ModRM: rcx ← [r10]  (mod=00 reg=1 rm=010)      */
/*--- CMP -----------------------------------------------------------------*/
#define CMP_RM_IMM8      0x83   /* cmp r/m, imm8  (group 1, /7)                   */
#define CMP_RM_IMM32     0x81   /* cmp r/m, imm32 (group 1, /7)                   */
#define MODRM_CMP_ESI    0xFE   /* ModRM: cmp esi, imm  (/7 → reg field = 7)      */
#define CMP_EAX_IMM32    0x3D   /* cmp eax, imm32 (short form)                    */
#define CMP_AL_IMM8      0x3C   /* cmp al,  imm8  (short form)                    */
#define CMP_R_RM         0x3B   /* cmp r32, r/m32                                  */
#define CMP_RM_R         0x39   /* cmp r/m32, r32                                  */
#define MODRM_CMP_RSP    0xC4   /* ModRM: cmp rsp, imm8  (mod=11 /7 rm=4)         */
/*--- ADD / SUB (short EAX forms) -----------------------------------------*/
#define ADD_EAX_IMM32    0x05   /* add eax, imm32                                  */
#define SUB_EAX_IMM32    0x2D   /* sub eax, imm32                                  */
/*--- XOR / TEST ----------------------------------------------------------*/
#define XOR_RM_R         0x31   /* xor r/m, r                                      */
#define MODRM_EAX_EAX    0xC0   /* ModRM: eax ↔ eax  (mod=11 reg=0 rm=0)           */
#define TEST_RM_R        0x85   /* test r/m, r                                     */
#define MODRM_RAX_RAX    0xC0   /* ModRM: rax ↔ rax  (same byte as EAX_EAX)        */
/*--- INC / CALL / PUSH group (FF) ----------------------------------------*/
#define INC_CALL_FF      0xFF   /* group 5: /0=inc /2=call /4=jmp /6=push          */
#define MODRM_CALL_RAX   0xD0   /* ModRM: call rax  (/2, mod=11, rm=0)             */
#define MODRM_INC_R13D   0x45   /* ModRM: inc dword [r13+disp8]  (/0, mod=01 rm=5) */
/*--- REX_B register extensions -------------------------------------------*/
#define REX_B_PUSH_R12   0x54   /* second byte of push r12  (REX.B 0x54)           */
#define REX_B_POP_R12    0x5C   /* second byte of pop r12   (REX.B 0x5C)           */
#define REX_B_PUSH_R10   0x52   /* second byte of push r10  (REX.B 0x52)           */
#define REX_B_POP_R10    0x5A   /* second byte of pop r10   (REX.B 0x5A)           */
/*--- PUSH / POP ----------------------------------------------------------*/
#define PUSH_RBP         0x55   /* push rbp                                        */
#define POP_RBP          0x5D   /* pop rbp                                         */
/*--- Branches ------------------------------------------------------------*/
#define JMP_REL8         0xEB   /* jmp rel8                                        */
#define JMP_REL32        0xE9   /* jmp rel32                                       */
#define JL_REL8          0x7C   /* jl  rel8                                        */
#define JGE_REL8         0x7D   /* jge rel8                                        */
#define JE_REL8          0x74   /* je  rel8                                        */
#define JNE_REL8         0x75   /* jne rel8                                        */
#define JNE_REL32_X      0x85   /* (0F 85) jne rel32                              */
#define JE_REL32_X       0x84   /* (0F 84) je  rel32                              */
#define JL_REL32_X       0x8C   /* (0F 8C) jl  rel32                              */
#define JGE_REL32_X      0x8D   /* (0F 8D) jge rel32                              */
#define JG_REL32_X       0x8F   /* (0F 8F) jg  rel32                              */
/*--- Misc ----------------------------------------------------------------*/
#define RET              0xC3   /* ret                                             */
#define NOP              0x90   /* nop                                             */
#endif
