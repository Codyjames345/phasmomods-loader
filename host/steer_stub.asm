; Direction substitution stub for Unity's crowd steering, patched at the map's UnityPlayer.SteerSite (steering.cpp).
;
; At that site RBX is the CrowdAgent being steered, XMM14, XMM15 and a slot of the stack frame hold its
; normalised steering direction X, Y and Z, and the replaced instruction reads its native desired speed
; into XMM0 (sdk/map/UnityPlayer.h). When RBX is the agent asked for and steering is active, only the
; direction is replaced; speed, braking, separation, avoidance and acceleration remain native. Otherwise it
; passes straight on.
;
; Everything the game holds comes from the map: steering.cpp copies this block, writes the frame slot into
; the displacement byte before pm_steer_replaced and the replaced instruction into pm_steer_replaced, and
; hands the copy to hooks::patch_site, which puts it in an executable page within rel32 range, with
; its jump back to the site's next instruction at the block's end. So the code ends with a jump to
; pm_steer_end and the data sits behind that jump, and every data reference is RIP-relative inside the
; block. No GPR or flag consumer is affected: the code following the site redefines flags before reading
; them.

PUBLIC pm_steer_begin
PUBLIC pm_steer_end
PUBLIC pm_steer_replaced
PUBLIC pm_steer_replaced_end
PUBLIC pm_steer_target
PUBLIC pm_steer_active
PUBLIC pm_steer_dx
PUBLIC pm_steer_dz

_TEXT SEGMENT ALIGN(16)

pm_steer_begin:
    cmp     rbx, qword ptr [pm_steer_target]
    jne     pm_steer_replaced
    cmp     dword ptr [pm_steer_active], 0
    je      pm_steer_replaced
    movss   xmm0, dword ptr [pm_steer_dx]
    movss   xmm14, xmm0
    movss   xmm0, dword ptr [pm_steer_zero]
    movss   xmm15, xmm0
    movss   xmm0, dword ptr [pm_steer_dz]
    movss   dword ptr [rsp+7Fh], xmm0         ; the frame slot: steering.cpp writes its displacement, this
                                              ; instruction's last byte
pm_steer_replaced:
    db      8 dup (90h)                       ; the replaced instruction: steering.cpp writes it
pm_steer_replaced_end:
    jmp     pm_steer_end

    ALIGN 8
pm_steer_target  dq 0
pm_steer_active  dd 0
pm_steer_dx      dd 0
pm_steer_dz      dd 0
pm_steer_zero    dd 0
pm_steer_end:

_TEXT ENDS
END
