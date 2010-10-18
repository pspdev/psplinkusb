
	.set noreorder
	.set noat

	.global psplinkSetK1
	.ent    psplinkSetK1

psplinkSetK1:
	move $v0, $k1
	jr	 $ra
	move $k1, $a0

	.end psplinkSetK1

# Try and do a fast copy with the vfpu
	.global copy16to16
	.ent	copy16to16
copy16to16:
	lui		$a2, 0x4000
	or		$a1, $a1, $a2

	li		$t1, 272
1:

# 480 * 2 / 64
	li		$t0, (480/32)

2:
	lv.q	C000, 0($a0)
	lv.q	C010, 16($a0)
	lv.q	C020, 32($a0)
	lv.q	C030, 48($a0)
	addiu	$t0, $t0, -1
	sv.q	C000, 0($a1), wb
	sv.q	C010, 16($a1), wb
	sv.q	C020, 32($a1), wb
	sv.q	C030, 48($a1), wb
	vnop
	addiu	$a0, $a0, 64
	bnez	$t0, 2b
	addiu	$a1, $a1, 64

	addiu	$t1, $t1, -1
	bnez	$t1, 1b
	addiu	$a0, $a0, (32*2)

	jr		$ra
	nop

	.end 	copy16to16

# Copy 32bit colour buffer to 16bit colour
# void copy32to16(unsigned int *in, unsigned short *out)
	.global copy32to16
	.ent	copy32to16

copy32to16:
# Each line is 480 * 4 / 32 units
# Need to increment 32 * 4 after

#	lui		$a2, 0x4000
#	or		$a1, $a1, $a2

	li		$t1, 272
1:
	li		$t0, (480/16)

2:
	lv.q	C000, 0($a0)
	lv.q	C010, 16($a0)
	lv.q	C020, 32($a0)
	lv.q	C030, 48($a0)
	vt5650.q C100, C000
	vt5650.q C102, C010
	vt5650.q C110, C020
	vt5650.q C112, C030
	sv.q	C100, 0($a1)
	sv.q	C110, 16($a1)
	addiu	$a0, $a0, 64
	addiu	$t0, $t0, -1
	bnez	$t0, 2b
	addiu	$a1, $a1, 32

	addiu	$t1, $t1, -1
	bnez	$t1, 1b
	addiu	$a0, $a0, (32*4)

	jr		$ra
	nop

	.end	copy32to16

