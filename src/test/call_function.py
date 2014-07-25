from rrutil import *

send_gdb('b breakpoint\n')
expect_gdb('Breakpoint 1')

send_gdb('c\n')
expect_gdb('Breakpoint 1, breakpoint')

send_gdb('call mutate_var()\n')
expect_gdb('var is 22')

send_gdb('call print_nums()\n')
expect_gdb('1 2 3 4 5')

send_gdb('call alloc_and_print()\n')
expect_gdb('Hello 22')

send_gdb('call make_unhandled_syscall()\n')
expect_gdb('return from splice: -1')

send_gdb('call print_time()\n')
# TODO: settle on a reasonable semantics for
# clock_gettime() during experiments
expect_gdb('now is 0 sec')

send_gdb('c\n')
expect_rr('var is -42')

ok()