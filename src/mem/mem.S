.intel_syntax noprefix
.section .text

.global strncpy
.global strncmp
.global memcmp
.global memmove
.global memcpy
.global memset
.global strlen 
.global strcmp

strncpy:
    mov rax, rdi
    test rdx, rdx
    jz .Lstrncpy_done

.Lstrncpy_copy:
    movzx rcx, byte ptr [rsi]
    mov [rdi], cl
    
    test rcx, rcx
    jz .Lstrncpy_pad
    
    inc rdi
    inc rsi
    dec rdx
    jnz .Lstrncpy_copy
    jmp .Lstrncpy_done

.Lstrncpy_pad:
    dec rdx
    jz .Lstrncpy_done
    inc rdi
.Lstrncpy_pad_loop:
    mov byte ptr [rdi], 0
    inc rdi
    dec rdx
    jnz .Lstrncpy_pad_loop

.Lstrncpy_done:
    ret

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
    push rdi
    mov rax, rsi
    mov rcx, rdx
    cld
    rep stosb
    pop rax
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
