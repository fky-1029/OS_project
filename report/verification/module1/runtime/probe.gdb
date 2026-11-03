set pagination off
set confirm off
set remotetimeout 5
target remote 127.0.0.1:12347
thbreak *lab3_module1_breakpoint
continue
set $break_pc = (unsigned long)&lab3_module1_breakpoint
set $resume_pc = (unsigned long)&lab3_module1_resume
set $pre_sp = $sp
set $pre_a0 = $a0
set $pre_t0 = $t0
set $pre_ra = $ra
printf "PRE_BREAK pc=%#lx resume=%#lx opcode=%#x sp=%#lx a0=%#lx t0=%#lx ra=%#lx sstatus=%#lx sie=%#lx\n", $pc, $resume_pc, *(unsigned int *)$pc, $pre_sp, $pre_a0, $pre_t0, $pre_ra, $sstatus, $sie
x/i $pc
if !(($pc==$break_pc))
  echo ASSERT_FAIL PRE_BREAK/1: ($pc==$break_pc)\n
  quit 1
end
if !((*(unsigned int*)$pc==0x00100073))
  echo ASSERT_FAIL PRE_BREAK/2: (*(unsigned int*)$pc==0x00100073)\n
  quit 1
end
if !(($resume_pc-$break_pc==4))
  echo ASSERT_FAIL PRE_BREAK/3: ($resume_pc-$break_pc==4)\n
  quit 1
end
if !(($pre_a0==0x3579b))
  echo ASSERT_FAIL PRE_BREAK/4: ($pre_a0==0x3579b)\n
  quit 1
end
if !(($pre_t0==0x2468a))
  echo ASSERT_FAIL PRE_BREAK/5: ($pre_t0==0x2468a)\n
  quit 1
end
if !(($pre_ra==0x13579))
  echo ASSERT_FAIL PRE_BREAK/6: ($pre_ra==0x13579)\n
  quit 1
end
if !(($sie==0))
  echo ASSERT_FAIL PRE_BREAK/7: ($sie==0)\n
  quit 1
end
if !((($sstatus&2)==2))
  echo ASSERT_FAIL PRE_BREAK/8: (($sstatus&2)==2)\n
  quit 1
end

thbreak *__alltraps
continue
set $original_sp = (unsigned long)$sp
printf "ASM_ENTRY pc=%#lx before_SAVE_ALL sp=%#lx sepc=%#lx scause=%#lx sstatus=%#lx\n", $pc, $original_sp, $sepc, $scause, $sstatus
x/i $pc
if !(($pc==(unsigned long)&__alltraps))
  echo ASSERT_FAIL ASM_ENTRY/1: ($pc==(unsigned long)&__alltraps)\n
  quit 1
end
if !(($original_sp==$pre_sp))
  echo ASSERT_FAIL ASM_ENTRY/2: ($original_sp==$pre_sp)\n
  quit 1
end
if !(($sepc==$break_pc))
  echo ASSERT_FAIL ASM_ENTRY/3: ($sepc==$break_pc)\n
  quit 1
end
if !(($scause==3))
  echo ASSERT_FAIL ASM_ENTRY/4: ($scause==3)\n
  quit 1
end

thbreak *trap
continue
set $frame = (unsigned long)$sp
set $tf = (struct trapframe *)$a0
set $frame_status = $tf->status
printf "C_ENTRY pc=%#lx sp=%#lx a0=%#lx saved_sp=%#lx frame_size=%ld status=%#lx epc=%#lx cause=%#lx saved_a0=%#lx saved_t0=%#lx saved_ra=%#lx call_ra=%#lx\n", $pc, $frame, $a0, $tf->gpr.sp, sizeof(*$tf), $frame_status, $tf->epc, $tf->cause, $tf->gpr.a0, $tf->gpr.t0, $tf->gpr.ra, $ra
x/i $pc
if !(($pc==(unsigned long)&trap))
  echo ASSERT_FAIL C_ENTRY/1: ($pc==(unsigned long)&trap)\n
  quit 1
end
if !(($a0==$frame))
  echo ASSERT_FAIL C_ENTRY/2: ($a0==$frame)\n
  quit 1
end
if !((sizeof(*$tf)==288))
  echo ASSERT_FAIL C_ENTRY/3: (sizeof(*$tf)==288)\n
  quit 1
end
if !(($original_sp-$frame==288))
  echo ASSERT_FAIL C_ENTRY/4: ($original_sp-$frame==288)\n
  quit 1
end
if !((($frame&15)==0))
  echo ASSERT_FAIL C_ENTRY/5: (($frame&15)==0)\n
  quit 1
end
if !(($tf->gpr.sp==$original_sp))
  echo ASSERT_FAIL C_ENTRY/6: ($tf->gpr.sp==$original_sp)\n
  quit 1
end
if !(($tf->gpr.a0==$pre_a0))
  echo ASSERT_FAIL C_ENTRY/7: ($tf->gpr.a0==$pre_a0)\n
  quit 1
end
if !(($tf->gpr.t0==$pre_t0))
  echo ASSERT_FAIL C_ENTRY/8: ($tf->gpr.t0==$pre_t0)\n
  quit 1
end
if !(($tf->gpr.ra==$pre_ra))
  echo ASSERT_FAIL C_ENTRY/9: ($tf->gpr.ra==$pre_ra)\n
  quit 1
end
if !(($tf->epc==$break_pc))
  echo ASSERT_FAIL C_ENTRY/10: ($tf->epc==$break_pc)\n
  quit 1
end
if !(($tf->cause==3))
  echo ASSERT_FAIL C_ENTRY/11: ($tf->cause==3)\n
  quit 1
end
if !((($frame_status&0x122)==0x120))
  echo ASSERT_FAIL C_ENTRY/12: (($frame_status&0x122)==0x120)\n
  quit 1
end
if !(($ra==(unsigned long)&__trapret))
  echo ASSERT_FAIL C_ENTRY/13: ($ra==(unsigned long)&__trapret)\n
  quit 1
end

thbreak *__trapret
continue
printf "C_RETURN pc=%#lx sp=%#lx frame=%#lx epc=%#lx sepc_before_restore=%#lx count=%u\n", $pc, $sp, $frame, $tf->epc, $sepc, lab3_module1_trap_count
if !(($pc==(unsigned long)&__trapret))
  echo ASSERT_FAIL C_RETURN/1: ($pc==(unsigned long)&__trapret)\n
  quit 1
end
if !(($sp==$frame))
  echo ASSERT_FAIL C_RETURN/2: ($sp==$frame)\n
  quit 1
end
if !(($tf->epc==$resume_pc))
  echo ASSERT_FAIL C_RETURN/3: ($tf->epc==$resume_pc)\n
  quit 1
end
if !(($sepc==$break_pc))
  echo ASSERT_FAIL C_RETURN/4: ($sepc==$break_pc)\n
  quit 1
end
if !((lab3_module1_trap_count==1))
  echo ASSERT_FAIL C_RETURN/5: (lab3_module1_trap_count==1)\n
  quit 1
end

thbreak *$sret_pc
continue
set $before_sret_status = $sstatus
printf "PRE_SRET pc=%#lx sepc=%#lx sp=%#lx a0=%#lx t0=%#lx ra=%#lx sstatus=%#lx SPP=%u SPIE=%u SIE=%u\n", $pc, $sepc, $sp, $a0, $t0, $ra, $sstatus, !!($sstatus&0x100), !!($sstatus&0x20), !!($sstatus&2)
x/i $pc
if !(($pc==$sret_pc))
  echo ASSERT_FAIL PRE_SRET/1: ($pc==$sret_pc)\n
  quit 1
end
if !((*(unsigned int*)$pc==0x10200073))
  echo ASSERT_FAIL PRE_SRET/2: (*(unsigned int*)$pc==0x10200073)\n
  quit 1
end
if !(($sepc==$resume_pc))
  echo ASSERT_FAIL PRE_SRET/3: ($sepc==$resume_pc)\n
  quit 1
end
if !(($sp==$original_sp))
  echo ASSERT_FAIL PRE_SRET/4: ($sp==$original_sp)\n
  quit 1
end
if !(($a0==$pre_a0))
  echo ASSERT_FAIL PRE_SRET/5: ($a0==$pre_a0)\n
  quit 1
end
if !(($t0==$pre_t0))
  echo ASSERT_FAIL PRE_SRET/6: ($t0==$pre_t0)\n
  quit 1
end
if !(($ra==$pre_ra))
  echo ASSERT_FAIL PRE_SRET/7: ($ra==$pre_ra)\n
  quit 1
end
if !(($before_sret_status==$frame_status))
  echo ASSERT_FAIL PRE_SRET/8: ($before_sret_status==$frame_status)\n
  quit 1
end

stepi
printf "POST_SRET pc=%#lx sp=%#lx sstatus=%#lx SPP=%u SPIE=%u SIE=%u\n", $pc, $sp, $sstatus, !!($sstatus&0x100), !!($sstatus&0x20), !!($sstatus&2)
if !(($pc==$resume_pc))
  echo ASSERT_FAIL POST_SRET resume PC\n
  quit 1
end
if !(($sp==$original_sp))
  echo ASSERT_FAIL POST_SRET original sp\n
  quit 1
end
# QEMU 4.1.1 helper_sret clears SPIE after copying it to SIE.
if !((($sstatus&0x122)==0x02))
  echo ASSERT_FAIL POST_SRET QEMU 4.1.1 SPP=0 SPIE=0 SIE=1\n
  quit 1
end
if !((lab3_module1_trap_count==1))
  echo ASSERT_FAIL POST_SRET exactly one trap\n
  quit 1
end
x/i $pc
stepi
printf "AFTER_RESUME pc=%#lx a0=%#lx\n", $pc, $a0
if !(($a0==0x3579c))
  echo ASSERT_FAIL resumed instruction executed\n
  quit 1
end
thbreak *lab3_module1_finished
continue
if !(($pc==(unsigned long)&lab3_module1_finished))
  echo ASSERT_FAIL test completed\n
  quit 1
end
if !((lab3_module1_trap_count==1))
  echo ASSERT_FAIL no repeated breakpoint\n
  quit 1
end
printf "GDB_MODULE1_PASS traps=%u\n", lab3_module1_trap_count
detach
quit
