# code to switch between user and kernel space.

	# kernel.ld causes this to be aligned
        # to a page boundary.
        #
#include "asm.h"
#include "trapframe.h"

.section u_eentry
.globl uservec
.align 4

uservec:    
	#
        # trap.c sets eentry to point here, so
        # traps from user space start here,
        # in privilege0, but with a
        # user page table.
        #
        # CSR.SAVE points to where the process's p->trapframe is
        # mapped into user space, at TRAPFRAME.
        #
        
	# swap a0 and SAVE
        # so that a0 is TRAPFRAME
        #csrwr  $a0, LOONGARCH_CSR_SAVE0

        csrwr  $t0, LOONGARCH_CSR_SAVE1
        csrrd  $t0, LOONGARCH_CSR_SAVE0
        csrwr  $a0, LOONGARCH_CSR_SAVE0
        add.d  $a0, $t0, $zero

        # save the user registers in TRAPFRAME
        st.d   $ra, $a0, TF_RA
        st.d   $tp, $a0, TF_TP
        st.d   $sp, $a0, TF_SP
        st.d   $a1, $a0, TF_A1
        st.d   $a2, $a0, TF_A2
        st.d   $a3, $a0, TF_A3
        st.d   $a4, $a0, TF_A4
        st.d   $a5, $a0, TF_A5
        st.d   $a6, $a0, TF_A6
        st.d   $a7, $a0, TF_A7
        csrrd  $t0, LOONGARCH_CSR_SAVE1
        st.d   $t0, $a0, TF_T0
        st.d   $t1, $a0, TF_T1
        st.d   $t2, $a0, TF_T2
        st.d   $t3, $a0, TF_T3
        st.d   $t4, $a0, TF_T4
        st.d   $t5, $a0, TF_T5
        st.d   $t6, $a0, TF_T6
        st.d   $t7, $a0, TF_T7
        st.d   $t8, $a0, TF_T8
        st.d   $r21, $a0, TF_R21
        st.d   $fp, $a0, TF_FP
        st.d   $s0, $a0, TF_S0
        st.d   $s1, $a0, TF_S1
        st.d   $s2, $a0, TF_S2
        st.d   $s3, $a0, TF_S3
        st.d   $s4, $a0, TF_S4
        st.d   $s5, $a0, TF_S5
        st.d   $s6, $a0, TF_S6
        st.d   $s7, $a0, TF_S7
        st.d   $s8, $a0, TF_S8

	# save the user a0 in p->trapframe->a0
        csrrd  $t0, LOONGARCH_CSR_SAVE0
        st.d   $t0, $a0, TF_A0

        # restore kernel stack pointer from p->trapframe->kernel_sp
        ld.d   $sp, $a0, TF_KERNEL_SP

        # make tp hold the current hartid, from p->trapframe->kernel_hartid
        ld.d   $tp, $a0, TF_KERNEL_HARTID

        # load the address of usertrap(), p->trapframe->kernel_trap
        ld.d   $t0, $a0, TF_KERNEL_TRAP

        # restore kernel page table from p->trapframe->kernel_pgdl
        ld.d   $t1, $a0, TF_KERNEL_PGDL
        csrwr  $t1, LOONGARCH_CSR_PGDL
        invtlb 0x0,$zero,$zero

        # jump to usertrap(), which does not return
        jirl   $zero ,$t0, 0

.globl userret
userret:
        # userret(TRAPFRAME, pagetable)
        # switch from kernel to user.
        # usertrapret() calls here.
        # a0: TRAPFRAME, in user page table.
        # a1: user page table, for pgdl.

        # switch to the user page table.
        csrwr  $a1, LOONGARCH_CSR_PGDL
        invtlb 0x0,$zero,$zero
        //ld.d   $t0, $a0, 264
        //csrrd $t0, 0x6

        # put the saved user a0 in SAVE0, so we
        # can swap it with our a0 (TRAPFRAME) in the last step.
        ld.d   $t0, $a0, TF_A0
        csrwr  $t0, LOONGARCH_CSR_SAVE0
        ld.d   $t0, $a0, TF_T0
        csrwr  $t0, LOONGARCH_CSR_SAVE1

        # restore all but a0 from TRAPFRAME

        ld.d   $ra, $a0, TF_RA
        ld.d   $tp, $a0, TF_TP
        ld.d   $sp, $a0, TF_SP
        ld.d   $a1, $a0, TF_A1
        ld.d   $a2, $a0, TF_A2
        ld.d   $a3, $a0, TF_A3
        ld.d   $a4, $a0, TF_A4
        ld.d   $a5, $a0, TF_A5
        ld.d   $a6, $a0, TF_A6
        ld.d   $a7, $a0, TF_A7
        ld.d   $t1, $a0, TF_T1
        ld.d   $t2, $a0, TF_T2
        ld.d   $t3, $a0, TF_T3
        ld.d   $t4, $a0, TF_T4
        ld.d   $t5, $a0, TF_T5
        ld.d   $t6, $a0, TF_T6
        ld.d   $t7, $a0, TF_T7
        ld.d   $t8, $a0, TF_T8
        ld.d   $r21, $a0, TF_R21
        ld.d   $fp, $a0, TF_FP
        ld.d   $s0, $a0, TF_S0
        ld.d   $s1, $a0, TF_S1
        ld.d   $s2, $a0, TF_S2
        ld.d   $s3, $a0, TF_S3
        ld.d   $s4, $a0, TF_S4
        ld.d   $s5, $a0, TF_S5
        ld.d   $s6, $a0, TF_S6
        ld.d   $s7, $a0, TF_S7
        ld.d   $s8, $a0, TF_S8

	# restore user a0, and save TRAPFRAME in SAVE0
        #csrwr  $a0, LOONGARCH_CSR_SAVE0
        csrrd  $t0, LOONGARCH_CSR_SAVE0
        csrwr  $a0, LOONGARCH_CSR_SAVE0
        add.d  $a0, $t0, $zero
        csrrd  $t0, LOONGARCH_CSR_SAVE1
        
        # return to user mode and user pc.
        # usertrapret() set up prmd and era.
        ertn
