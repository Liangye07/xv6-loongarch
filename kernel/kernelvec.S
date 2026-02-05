        #
        # interrupts and exceptions while in privilege0 (kernel)
        # push registers, call kerneltrap(), restore, return.
        #
        .section k_eentry
        .globl kerneltrap
        .globl kernelvec
        .align 4

kernelvec:
        # 为保存寄存器在内核栈上分配空间（栈向下生长）
        addi.d  $sp, $sp, -256

        # 保存寄存器（不要保存 $sp 本身；不要保存/恢复 $tp 若其包含 hartid）
        st.d    $ra, $sp, 0
        st.d    $tp, $sp, 8        # 可选保存（通常不恢复），若你选择不保存可删掉此行
        # st.d $sp, $sp, 16       # 不要保存 $sp 到栈上（会引入混乱），所以注释掉
        st.d    $a0, $sp, 24
        st.d    $a1, $sp, 32
        st.d    $a2, $sp, 40
        st.d    $a3, $sp, 48
        st.d    $a4, $sp, 56
        st.d    $a5, $sp, 64
        st.d    $a6, $sp, 72
        st.d    $a7, $sp, 80
        st.d    $t0, $sp, 88
        st.d    $t1, $sp, 96
        st.d    $t2, $sp, 104
        st.d    $t3, $sp, 112
        st.d    $t4, $sp, 120
        st.d    $t5, $sp, 128
        st.d    $t6, $sp, 136
        st.d    $t7, $sp, 144
        st.d    $t8, $sp, 152
        st.d    $r21, $sp, 160
        st.d    $fp, $sp, 168
        st.d    $s0, $sp, 176
        st.d    $s1, $sp, 184
        st.d    $s2, $sp, 192
        st.d    $s3, $sp, 200
        st.d    $s4, $sp, 208
        st.d    $s5, $sp, 216
        st.d    $s6, $sp, 224
        st.d    $s7, $sp, 232
        st.d    $s8, $sp, 240

        # 调用 C 处理器（kerneltrap）
        bl      kerneltrap

        # 恢复寄存器（注意：如果你保存了 $tp，上面没恢复以避免跨 hart 恢复）
        ld.d    $ra, $sp, 0
        # 如果保存了 $tp 并且要恢复，请把下面注释去掉；通常建议不恢复 $tp
        # ld.d $tp, $sp, 8
        ld.d    $a0, $sp, 24
        ld.d    $a1, $sp, 32
        ld.d    $a2, $sp, 40
        ld.d    $a3, $sp, 48
        ld.d    $a4, $sp, 56
        ld.d    $a5, $sp, 64
        ld.d    $a6, $sp, 72
        ld.d    $a7, $sp, 80
        ld.d    $t0, $sp, 88
        ld.d    $t1, $sp, 96
        ld.d    $t2, $sp, 104
        ld.d    $t3, $sp, 112
        ld.d    $t4, $sp, 120
        ld.d    $t5, $sp, 128
        ld.d    $t6, $sp, 136
        ld.d    $t7, $sp, 144
        ld.d    $t8, $sp, 152
        ld.d    $r21, $sp, 160
        ld.d    $fp, $sp, 168
        ld.d    $s0, $sp, 176
        ld.d    $s1, $sp, 184
        ld.d    $s2, $sp, 192
        ld.d    $s3, $sp, 200
        ld.d    $s4, $sp, 208
        ld.d    $s5, $sp, 216
        ld.d    $s6, $sp, 224
        ld.d    $s7, $sp, 232
        ld.d    $s8, $sp, 240

        addi.d  $sp, $sp, 256

        # 返回到异常发生处（LoongArch: ERTN）
        ertn
