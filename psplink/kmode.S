
	.set noreorder
	.set noat

	.global psplinkSetK1
	.ent    psplinkSetK1

psplinkSetK1:
	move $v0, $k1
	jr	 $ra
	move $k1, $a0

	.end psplinkSetK1

	.global psplinkGetCop0
	.ent    psplinkGetCop0

psplinkGetCop0:
	mfc0	$v0, $0
	sw		$v0, 0($a0)
	mfc0	$v0, $1
	sw		$v0, 4($a0)
	mfc0	$v0, $2
	sw		$v0, 8($a0)
	mfc0	$v0, $3
	sw		$v0, 12($a0)
	mfc0	$v0, $4
	sw		$v0, 16($a0)
	mfc0	$v0, $5
	sw		$v0, 20($a0)
	mfc0	$v0, $6
	sw		$v0, 24($a0)
	mfc0	$v0, $7
	sw		$v0, 28($a0)
	mfc0	$v0, $8
	sw		$v0, 32($a0)
	mfc0	$v0, $9
	sw		$v0, 36($a0)
	mfc0	$v0, $10
	sw		$v0, 40($a0)
	mfc0	$v0, $11
	sw		$v0, 44($a0)
	mfc0	$v0, $12
	sw		$v0, 48($a0)
	mfc0	$v0, $13
	sw		$v0, 52($a0)
	mfc0	$v0, $14
	sw		$v0, 56($a0)
	mfc0	$v0, $15
	sw		$v0, 60($a0)
	mfc0	$v0, $16
	sw		$v0, 64($a0)
	mfc0	$v0, $17
	sw		$v0, 68($a0)
	mfc0	$v0, $18
	sw		$v0, 72($a0)
	mfc0	$v0, $19
	sw		$v0, 76($a0)
	mfc0	$v0, $20
	sw		$v0, 80($a0)
	mfc0	$v0, $21
	sw		$v0, 84($a0)
	mfc0	$v0, $22
	sw		$v0, 88($a0)
	mfc0	$v0, $23
	sw		$v0, 92($a0)
	mfc0	$v0, $24
	sw		$v0, 96($a0)
	mfc0	$v0, $25
	sw		$v0, 100($a0)
	mfc0	$v0, $26
	sw		$v0, 104($a0)
	mfc0	$v0, $27
	sw		$v0, 108($a0)
	mfc0	$v0, $28
	sw		$v0, 112($a0)
	mfc0	$v0, $29
	sw		$v0, 116($a0)
	mfc0	$v0, $30
	sw		$v0, 120($a0)
	mfc0	$v0, $31
	sw		$v0, 124($a0)
	addiu	$a0, $a0, 128
	cfc0	$v0, $0
	sw		$v0, 0($a0)
	cfc0	$v0, $1
	sw		$v0, 4($a0)
	cfc0	$v0, $2
	sw		$v0, 8($a0)
	cfc0	$v0, $3
	sw		$v0, 12($a0)
	cfc0	$v0, $4
	sw		$v0, 16($a0)
	cfc0	$v0, $5
	sw		$v0, 20($a0)
	cfc0	$v0, $6
	sw		$v0, 24($a0)
	cfc0	$v0, $7
	sw		$v0, 28($a0)
	cfc0	$v0, $8
	sw		$v0, 32($a0)
	cfc0	$v0, $9
	sw		$v0, 36($a0)
	cfc0	$v0, $10
	sw		$v0, 40($a0)
	cfc0	$v0, $11
	sw		$v0, 44($a0)
	cfc0	$v0, $12
	sw		$v0, 48($a0)
	cfc0	$v0, $13
	sw		$v0, 52($a0)
	cfc0	$v0, $14
	sw		$v0, 56($a0)
	cfc0	$v0, $15
	sw		$v0, 60($a0)
	cfc0	$v0, $16
	sw		$v0, 64($a0)
	cfc0	$v0, $17
	sw		$v0, 68($a0)
	cfc0	$v0, $18
	sw		$v0, 72($a0)
	cfc0	$v0, $19
	sw		$v0, 76($a0)
	cfc0	$v0, $20
	sw		$v0, 80($a0)
	cfc0	$v0, $21
	sw		$v0, 84($a0)
	cfc0	$v0, $22
	sw		$v0, 88($a0)
	cfc0	$v0, $23
	sw		$v0, 92($a0)
	cfc0	$v0, $24
	sw		$v0, 96($a0)
	cfc0	$v0, $25
	sw		$v0, 100($a0)
	cfc0	$v0, $26
	sw		$v0, 104($a0)
	cfc0	$v0, $27
	sw		$v0, 108($a0)
	cfc0	$v0, $28
	sw		$v0, 112($a0)
	cfc0	$v0, $29
	sw		$v0, 116($a0)
	cfc0	$v0, $30
	sw		$v0, 120($a0)
	cfc0	$v0, $31
	sw		$v0, 124($a0)
	jr		$ra
	nop

	.end psplinkGetCop0

	.global _apiHookHandle
	.global _apiHookReturn

	.global _apiHookEntry
	.ent    _apiHookEntry
_apiHookEntry:
	addiu	$sp, $sp, -(4*8) # Allocate space for 6 args + $ra + hooknum
	sw		$ra, 0($sp)		 # Save $ra and args
	sw		$a0, 4($sp)
	sw		$a1, 8($sp)
	sw		$a2, 12($sp)
	sw		$a3, 16($sp)
	sw		$t0, 20($sp)
	sw		$t1, 24($sp)
	sw		$v0, 28($sp)
	move	$a0, $v0		# Copy api hook number into first arg
	jal		_apiHookHandle
	addiu	$a1, $sp, 4		# Copy pointer to args into second arg

	lw		$a0, 4($sp)  # Restore args
	lw		$a1, 8($sp)
	lw		$a2, 12($sp)
	lw		$a3, 16($sp)

	beq		$v0, $0, 1f
	lw		$t0, 20($sp)

	jalr	$v0				# Will call with a 32 byte overhead
	lw		$t1, 24($sp)

	sw		$v0, 4($sp)		# Save return values
	sw		$v1, 8($sp)
	lw		$a0, 28($sp)	# Load hook address

	jal		_apiHookReturn  # Print return values
	addiu	$a1, $sp, 4		# Copy pointer to return values into second arg

	lw		$v0, 4($sp)		# Restore return values
	lw		$v1, 8($sp)

1:
	lw		$ra, 0($sp)		 # Restore $ra
	jr		$ra
	addiu	$sp, $sp, (4*8)
	.end    _apiHookEntry

	.global _apiHook0
	.ent    _apiHook0
_apiHook0:
	j		_apiHookEntry
	li		$v0, 0
	.end    _apiHook0
	
	.global _apiHook1
	.ent    _apiHook1
_apiHook1:
	j		_apiHookEntry
	li		$v0, 1
	.end    _apiHook1

	.global _apiHook2
	.ent    _apiHook2
_apiHook2:
	j		_apiHookEntry
	li		$v0, 2
	.end    _apiHook2

	.global _apiHook3
	.ent    _apiHook3
_apiHook3:
	j		_apiHookEntry
	li		$v0, 3
	.end    _apiHook3

	.global _apiHook4
	.ent    _apiHook4
_apiHook4:
	j		_apiHookEntry
	li		$v0, 4
	.end    _apiHook4

	.global _apiHook5
	.ent    _apiHook5
_apiHook5:
	j		_apiHookEntry
	li		$v0, 5
	.end    _apiHook5

	.global _apiHook6
	.ent    _apiHook6
_apiHook6:
	j		_apiHookEntry
	li		$v0, 6
	.end    _apiHook6

	.global _apiHook7
	.ent    _apiHook7
_apiHook7:
	j		_apiHookEntry
	li		$v0, 7
	.end    _apiHook7

	.global _apiHook8
	.ent    _apiHook8
_apiHook8:
	j		_apiHookEntry
	li		$v0, 8
	.end    _apiHook8

	.global _apiHook9
	.ent    _apiHook9
_apiHook9:
	j		_apiHookEntry
	li		$v0, 9
	.end    _apiHook9

	.global _apiHook10
	.ent    _apiHook10
_apiHook10:
	j		_apiHookEntry
	li		$v0, 10
	.end    _apiHook10

	.global _apiHook11
	.ent    _apiHook11
_apiHook11:
	j		_apiHookEntry
	li		$v0, 11
	.end    _apiHook11

	.global _apiHook12
	.ent    _apiHook12
_apiHook12:
	j		_apiHookEntry
	li		$v0, 12
	.end    _apiHook12

	.global _apiHook13
	.ent    _apiHook13
_apiHook13:
	j		_apiHookEntry
	li		$v0, 13
	.end    _apiHook13

	.global _apiHook14
	.ent    _apiHook14
_apiHook14:
	j		_apiHookEntry
	li		$v0, 14
	.end    _apiHook14
	
	.global _apiHook15
	.ent    _apiHook15
_apiHook15:
	j		_apiHookEntry
	li		$v0, 15
	.end    _apiHook15
