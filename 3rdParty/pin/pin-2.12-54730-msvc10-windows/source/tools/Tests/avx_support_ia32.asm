PUBLIC HasAvxSupport



.686
.model flat, c
.XMM

.code
 ALIGN 4 
 HasAvxSupport PROC
    push    ebp
    mov     ebp, esp
    pusha

    mov     eax, 1

    cpuid

    and ecx, 0018000000h
    cmp ecx, 0018000000h 
    jne $lNOT_SUPPORTED 
    mov ecx, 0          
    
    BYTE 00Fh
    BYTE 001h
    BYTE 0D0h
    and eax, 6
    cmp eax, 6        
    jne $lNOT_SUPPORTED
    popa
    mov eax, 1
    jmp $lDONE
$lNOT_SUPPORTED:
    popa
    mov eax, 0
$lDONE:
    mov     esp, ebp
    pop     ebp
    ret
HasAvxSupport ENDP


end
