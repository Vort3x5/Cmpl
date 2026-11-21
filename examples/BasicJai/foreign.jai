// Jai-style FFI (The_Way_to_Jai §29)
// Expected: prints "Hi\n", exit 3

cmpl_rt :: #library "cmpl_rt";

sys_write :: (fd: int, buf: int, count: int) -> int #foreign cmpl_rt;
msg_hi_ptr :: () -> int #foreign cmpl_rt;
msg_hi_len :: () -> int #foreign cmpl_rt;

main :: () -> int {
    buf := msg_hi_ptr();
    n := msg_hi_len();
    written := sys_write(1, buf, n);
    return written;
}
