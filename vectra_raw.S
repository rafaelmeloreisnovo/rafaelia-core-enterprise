.global _start

// ============================
// ENTRYPOINT REAL (sem libc)
// ============================
_start:

    // ============================
    // setup buffer (stack)
    // ============================
    sub sp, sp, #4096         // reserva 4KB
    mov x0, sp                // x0 = buffer
    mov x1, #4096             // tamanho

// ============================
// FILL BUFFER (pseudo-rand)
// ============================
fill:
    subs x1, x1, #1
    eor x2, x1, x1, lsl #13
    strb w2, [x0, x1]
    b.ne fill

// ============================
// CRC32 CORE
// ============================
    mov x0, sp                // buffer
    mov x1, #4096             // size
    mov w2, #0                // CRC acumulado

loop:
    cmp x1, #8
    blt tail

    ldr x3, [x0], #8          // load 64 bits
    crc32x w2, w2, x3         // HW CRC

    subs x1, x1, #8
    b.ge loop

tail:
    cmp x1, #0
    beq done

    ldrb w3, [x0], #1
    crc32b w2, w2, w3

    subs x1, x1, #1
    b.ne tail

// ============================
// OUTPUT (write syscall)
// ============================
done:
    // converte CRC → ASCII HEX (manual)
    mov x4, sp
    add x4, x4, #2048         // espaço no meio do buffer

    mov x5, #8                // 8 hex chars
    mov w6, w2

hex_loop:
    and w7, w6, #0xF
    cmp w7, #10
    add w7, w7, #'0'
    csel w7, w7, w7, lt
    add w7, w7, #39           // ajusta A-F

    strb w7, [x4, x5]
    lsr w6, w6, #4

    subs x5, x5, #1
    b.ge hex_loop

    // newline
    mov w7, #10
    strb w7, [x4, #9]

    // write(fd=1, buf, len)
    mov x0, #1
    mov x1, x4
    mov x2, #10
    mov x8, #64               // syscall write
    svc #0

// ============================
// exit
// ============================
    mov x0, #0
    mov x8, #93               // syscall exit
    svc #0

