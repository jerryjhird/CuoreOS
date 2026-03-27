.intel_syntax noprefix
.section .text

.global memcpy
.global memmove
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
    mov r8, rax
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
    jb .Lforward

    std
    lea rdi, [rdi + rdx - 1]
    lea rsi, [rsi + rdx - 1]
    mov rcx, rdx
    rep movsb
    cld
    ret

.Lforward:
    cld
    mov rcx, rdx
    rep movsb
.Ldone:
    ret