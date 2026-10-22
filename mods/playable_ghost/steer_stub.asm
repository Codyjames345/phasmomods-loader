; Direction substitution stub for UnityPlayer crowd steering (build cd106e0f...).
;
; Patched site: UnityPlayer+0xA4FB7A  `movss xmm0, dword ptr [rbx+290h]` (8 bytes), replaced
; with `jmp stub` + 3 NOPs. At that point RBX is the CrowdAgent being steered, XMM14/XMM15/
; [RSP+24h] hold its normalised steering direction X/Y/Z and [RBX+290h] its native desired
; speed. When RBX is the owned ghost agent and steering is active, only the direction is
; replaced; speed, braking, separation, avoidance and acceleration remain native.
;
; The block is copied verbatim into an executable page within rel32 range of UnityPlayer, so
; every data reference is RIP-relative inside the block. No GPR or flag consumer is affected:
; the code following the site redefines flags (cmp r14,4) before reading them.

PUBLIC pg_steer_begin
PUBLIC pg_steer_end
PUBLIC pg_steer_target
PUBLIC pg_steer_active
PUBLIC pg_steer_dx
PUBLIC pg_steer_dz
PUBLIC pg_steer_resume

_TEXT SEGMENT ALIGN(16)

pg_steer_begin:
    cmp     rbx, qword ptr [pg_steer_target]
    jne     original
    cmp     dword ptr [pg_steer_active], 0
    je      original
    movss   xmm0, dword ptr [pg_steer_dx]
    movss   xmm14, xmm0
    movss   xmm0, dword ptr [pg_steer_zero]
    movss   xmm15, xmm0
    movss   xmm0, dword ptr [pg_steer_dz]
    movss   dword ptr [rsp+24h], xmm0
original:
    movss   xmm0, dword ptr [rbx+290h]
    jmp     qword ptr [pg_steer_resume]

    ALIGN 8
pg_steer_target  dq 0
pg_steer_resume  dq 0
pg_steer_active  dd 0
pg_steer_dx      dd 0
pg_steer_dz      dd 0
pg_steer_zero    dd 0
pg_steer_end:

_TEXT ENDS
END
