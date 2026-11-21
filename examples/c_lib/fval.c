/* SSE-sensitive callee: misaligned RSP before call → segfault on movaps. */
long fval(long x)
{
    /* Force 16-byte aligned stack spill; crashes if caller left RSP ≡ 8 mod 16. */
    volatile __attribute__((aligned(16))) float scratch[4] = {0, 0, 0, 0};
    __asm__ volatile(
        "movaps %0, %%xmm0\n\t"
        "movaps %%xmm0, %0\n\t"
        : "+m"(scratch)
        :
        : "xmm0");
    return x + 1;
}
