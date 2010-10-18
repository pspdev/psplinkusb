# Reboot hooker, patches out the plain text prx check so you can loadexec
# any valid PSP module.
# NOTE: This is only for the v1.5 kernel
# (c) TyRaNiD 2k6 (inspired by umdemu)

	.set noreorder
	.set noat

	.global _stage

	.global _start
	.ent    _start

# _start is called instead of the reboot code
_start:
# Patch in a jump to _stage at the appropriate point in the reboot code
	li	$t1, 0x0a0f8005
	lui	$t2, 0x88c0
	j	0x8C00000
	sw	$t1, 0xFF8($t2)

	.end _start

	.ent	_stage

# Patch out the check in loadcore which refuses to load plain text modules
_stage:
	li	$t1, 0x88010000
	li	$t2, 0x340D0001
	jr	$v0
	sw	$t2, 0x52e0($t1)

	.end _stage
