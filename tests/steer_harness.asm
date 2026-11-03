; Runs the host's steering stub the way UnityPlayer's crowd update reaches it, for tests/movement_test.cpp: RBX the
; agent, XMM14, XMM15 and a slot of the frame the direction, and the code at the patched site runs from its first
; byte, so through the host's jump to the stub and its jump back. After that has run it hands back what the code
; following the site would read.
;
; void pm_steer_run(void* site, void* agent, float* x, float* y, float* z, float* speed, int frameSlot)
;
; `frameSlot` is the slot's offset from RSP at the site, which CALL's return address puts 8 below this
; frame's RSP. It is a byte, and the frame has room for it.

PUBLIC pm_steer_run

_TEXT SEGMENT

pm_steer_run PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 180h
    .allocstack 180h
    movups  xmmword ptr [rsp+160h], xmm14
    .savexmm128 xmm14, 160h
    movups  xmmword ptr [rsp+170h], xmm15
    .savexmm128 xmm15, 170h
    .endprolog

    mov     rax, rcx                          ; the site
    mov     rbx, rdx                          ; the agent
    mov     rsi, r8                           ; x
    mov     rdi, r9                           ; y
    mov     r10, qword ptr [rsp+180h+18h+28h] ; z
    mov     r11d, dword ptr [rsp+180h+18h+38h] ; the frame slot, from RSP at the site
    sub     r11, 8                            ; from RSP here
    movss   xmm14, dword ptr [rsi]
    movss   xmm15, dword ptr [rdi]
    movss   xmm0, dword ptr [r10]
    movss   dword ptr [rsp+r11], xmm0
    xorps   xmm0, xmm0
    call    rax

    movss   dword ptr [rsi], xmm14
    movss   dword ptr [rdi], xmm15
    mov     r10, qword ptr [rsp+180h+18h+28h]
    mov     r11d, dword ptr [rsp+180h+18h+38h]
    sub     r11, 8
    movss   xmm1, dword ptr [rsp+r11]
    movss   dword ptr [r10], xmm1
    mov     r10, qword ptr [rsp+180h+18h+30h] ; speed
    movss   dword ptr [r10], xmm0

    movups  xmm14, xmmword ptr [rsp+160h]
    movups  xmm15, xmmword ptr [rsp+170h]
    add     rsp, 180h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
pm_steer_run ENDP

_TEXT ENDS
END
