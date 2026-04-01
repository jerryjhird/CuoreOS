.intel_syntax noprefix
.section .text

.global strncmp
.global memcmp
.global memmove
.global memcpy
.global memset
.global strlen 
.global strcmp

strncmp:
    xor rax, rax
    test rdx, rdx
    jz .Lstrncmp_equal

.Lstrncmp_loop:
    mov al, [rdi]
    mov cl, [rsi]

    cmp al, cl
    jne .Lstrncmp_diff

    test al, al
    jz .Lstrncmp_equal

    dec rdx
    jz .Lstrncmp_equal

    inc rdi
    inc rsi
    jmp .Lstrncmp_loop

.Lstrncmp_diff:
    movzx rax, al
    movzx rcx, cl
    sub rax, rcx
    ret

.Lstrncmp_equal:
    xor rax, rax
    ret

strcmp:
    xor rax, rax
.Lloop:
    mov al, [rdi]
    mov dl, [rsi]
    cmp al, dl
    jne .Ldiff
    test al, al
    jz .Lequal
    inc rdi
    inc rsi
    jmp .Lloop
.Ldiff:
    movzx rax, al
    movzx rdx, dl
    sub rax, rdx
    ret
.Lequal:
    xor rax, rax
    ret

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