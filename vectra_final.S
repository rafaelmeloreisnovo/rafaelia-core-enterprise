// ==================================================================
// VECTRA FINAL – Hardware detection + pipeline stages + clean output
// ==================================================================

.global _start

// --------------------------------------------------------------
// Mapeamento de registradores
// --------------------------------------------------------------
// x19 : buffer base
// x20 : tamanho (4096)
// w21 : invariante (CRC acumulado)
// x22 : delta (ciclos totais)
// w23 : contador de ciclos (56)
// x24 : temporário para leitura de tempo
// x25 : endereço de trabalho
// --------------------------------------------------------------

// --------------------------------------------------------------
// Verificação de hardware
// --------------------------------------------------------------
check_crc:
    mrs     x0, ID_AA64ISAR0_EL1
    ubfx    x0, x0, #8, #4
    cmp     x0, #1
    b.ge    1f
    adr     x1, err_crc
    mov     x2, 30
    b       abort
1:  ret

check_neon:
    mrs     x0, ID_AA64PFR0_EL1
    ubfx    x0, x0, #16, #4
    cmp     x0, #1
    b.ge    1f
    adr     x1, err_neon
    mov     x2, 30
    b       abort
1:  ret

check_cycles:
    isb
    mrs     x0, cntvct_el0
    cmp     x0, #0
    b.ne    1f
    adr     x1, err_cycles
    mov     x2, 30
    b       abort
1:  ret

abort:
    mov     x0, #1
    mov     x8, #64
    svc     #0
    mov     x0, #1
    mov     x8, #93
    svc     #0

// --------------------------------------------------------------
// Preenchimento do buffer
// --------------------------------------------------------------
fill_buffer:
    mov     x3, x0          // addr
    mov     x4, x1          // size
    mov     x5, x2          // seed
1:
    subs    x4, x4, #8
    str     x5, [x3], #8
    add     x5, x5, #0x9E3779B9
    b.ne    1b
    ret

// --------------------------------------------------------------
// CRC32 (invariante) – otimizado 16 bytes/iter
// --------------------------------------------------------------
compute_crc:
    mov     w2, #0
    mov     x3, x0
    mov     x4, x1
1:
    cmp     x4, #16
    blt     2f
    ldp     x5, x6, [x3], #16
    crc32x  w2, w2, x5
    crc32x  w2, w2, x6
    sub     x4, x4, #16
    b       1b
2:
    cbz     x4, 3f
    ldrb    w5, [x3], #1
    crc32b  w2, w2, w5
    sub     x4, x4, #1
    b       2b
3:
    mov     w0, w2
    ret

// --------------------------------------------------------------
// NEON mix (variante) – correto e pipeline-friendly
// --------------------------------------------------------------
neon_mix:
    mov     x2, x0
    mov     x3, x1
1:
    subs    x3, x3, #64
    ld1     {v0.16b, v1.16b, v2.16b, v3.16b}, [x2]
    ext     v4.16b, v0.16b, v1.16b, #7
    ext     v5.16b, v2.16b, v3.16b, #11
    eor     v0.16b, v0.16b, v5.16b
    eor     v2.16b, v2.16b, v4.16b
    st1     {v0.16b, v1.16b, v2.16b, v3.16b}, [x2], #64
    b.ne    1b
    ret

// --------------------------------------------------------------
// Conversão para hex (4 bytes → 8 hex chars)
// --------------------------------------------------------------
u32_to_hex:
    // x0 = buffer (8 bytes), w1 = valor
    mov     w2, w1
    mov     x3, x0
    mov     w4, #8
    adr     x5, hex_table
1:
    and     w6, w2, #0xF
    ldrb    w6, [x5, w6]
    strb    w6, [x3, w4-1]!   // pré-decremento
    lsr     w2, w2, #4
    subs    w4, w4, #1
    b.ne    1b
    ret
hex_table:
    .byte '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'

// --------------------------------------------------------------
// SAÍDA FORMATADA
// --------------------------------------------------------------
output_hex64:
    // x0 = valor (64 bits), buffer em x1, escreve 16 hex chars
    mov     x2, x0
    mov     x3, x1
    mov     w4, #16
1:
    and     w5, w2, #0xF
    ldrb    w5, [x6, w5]       // x6 deve apontar para hex_table
    strb    w5, [x3, w4-1]!
    lsr     x2, x2, #4
    subs    w4, w4, #1
    b.ne    1b
    ret

// --------------------------------------------------------------
// MAIN
// --------------------------------------------------------------
_start:
    // verifica hardware
    bl      check_crc
    bl      check_neon
    bl      check_cycles

    // reserva stack (4KB + espaço para string de saída)
    sub     sp, sp, #4096+64
    mov     x19, sp
    add     x20, x19, #64      // buffer começa após espaço para saída

    // preenche buffer
    mov     x0, x20
    mov     x1, #4096
    mov     x2, #0x9E3779B97F4A7C15
    bl      fill_buffer

    // estado inicial
    mov     w21, #0
    mov     x22, #0
    mov     w23, #56

main_loop:
    cbz     w23, finish

    // --- invariante ---
    isb
    mrs     x24, cntvct_el0
    mov     x0, x20
    mov     x1, #4096
    bl      compute_crc
    isb
    mrs     x25, cntvct_el0
    sub     x25, x25, x24
    add     x22, x22, x25
    eor     w21, w21, w0

    // --- variante ---
    isb
    mrs     x24, cntvct_el0
    mov     x0, x20
    mov     x1, #4096
    bl      neon_mix
    isb
    mrs     x25, cntvct_el0
    sub     x25, x25, x24
    add     x22, x22, x25

    sub     w23, w23, #1
    b       main_loop

finish:
    // formata saída: "CRC:xxxxxxxx CYCLES:yyyyyyyy\n"
    adr     x0, output_buffer
    mov     x1, #0
    // escreve "CRC:"
    adr     x2, str_crc
    bl      strcpy
    // converte CRC (w21) para hex em x0
    mov     w1, w21
    bl      u32_to_hex
    add     x0, x0, #8
    // escreve " CYCLES:"
    adr     x2, str_cycles
    bl      strcpy
    // converte x22 para hex (16 chars)
    mov     x1, x22
    bl      u64_to_hex
    add     x0, x0, #16
    // newline
    mov     w1, #10
    strb    w1, [x0], #1
    // syscall write
    adr     x1, output_buffer
    sub     x2, x0, x1
    mov     x0, #1
    mov     x8, #64
    svc     #0

    // restaura stack e sai
    add     sp, sp, #4096+64
    mov     x0, #0
    mov     x8, #93
    svc     #0

// --------------------------------------------------------------
// Utilitários de string
// --------------------------------------------------------------
strcpy:
    // x0 = destino, x2 = origem, retorna x0 = próximo após cópia
1:
    ldrb    w3, [x2], #1
    strb    w3, [x0], #1
    cbnz    w3, 1b
    sub     x0, x0, #1
    ret

u64_to_hex:
    // x0 = buffer (16 bytes), x1 = valor
    mov     x2, x1
    mov     x3, x0
    mov     w4, #16
    adr     x5, hex_table
1:
    and     w6, w2, #0xF
    ldrb    w6, [x5, w6]
    strb    w6, [x3, w4-1]!
    lsr     x2, x2, #4
    subs    w4, w4, #1
    b.ne    1b
    add     x0, x0, #16
    ret

// --------------------------------------------------------------
// Dados
// --------------------------------------------------------------
.section .rodata
str_crc:    .asciz "CRC:"
str_cycles: .asciz " CYCLES:"
err_crc:    .asciz "ERRO: CPU sem extensão CRC32\n"
err_neon:   .asciz "ERRO: CPU sem NEON\n"
err_cycles: .asciz "ERRO: Contador de ciclos indisponível\n"

.section .bss
output_buffer: .space 128
