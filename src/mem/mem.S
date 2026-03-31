.intel_syntax noprefix
.section .text

.global memcmp
.global memmove
.global memcpy
.global memset
.global strlen 

strlen:
    mov rax, rdi
    xor rcx, rcx
    dec rcx
    xor al, al
    cld
    repne scasb
    not rcx
    dec rcx
    mov rax, rcx
    ret

memset:
    mov rax, rdi
    mov rcx, rdx
    mov al, sil
    cld
    rep stosb
    ret

memcpy:
    mov rax, rdi
    mov rcx, rdx
    cld
    rep movsb
    ret

memmove:
    mov rax, rdi
    cmp rdi, rsi
    je .Ldone
    jb memcpy

    std
    lea rdi, [rdi + rdx - 1]
    lea rsi, [rsi + rdx - 1]
    mov rcx, rdx
    rep movsb
    cld
    ret

memcmp:
    mov rcx, rdx
    xor rax, rax
    test rcx, rcx
    jz .Ldone

    cld
    repe cmpsb
    
    je .Ldone

    movzx rax, byte ptr [rdi - 1]
    movzx rdx, byte ptr [rsi - 1]
    sub rax, rdx
    ret

.Ldone:
    ret