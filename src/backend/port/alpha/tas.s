	.ugen	
	.verstamp	3 11
	.text	
	.align	4
	.file	2 "tas.c"
	.globl	tas
	.loc	2 20
 #   20	{
	.ent	tas 2
tas:
	.option	O1
	ldgp	$gp, 0($27)
	.frame	$sp, 0, $26, 0
	.prologue	1
	.loc	2 20

	.loc	2 21
 #   21		if (*lock == 0) {
	ldq_l	$1, 0($16)
	bne	$1, $32
	.loc	2 21

	.loc	2 22
 #   22			*lock = 1;
	ldiq	$2, 1
	stq_c	$2, 0($16)
	.loc	2 23
 #   23			if (*lock)
	beq	$2, $32
	.loc	2 24
 #   24				return 0;
	bis	$31, $31, $0
	.livereg	0x807F0002,0x3FC00000
	ret	$31, ($26), 1
$32:
	.loc	2 26
 #   25		}
 #   26		return 1;
	ldil	$0, 1
$33:
	.livereg	0xFC7F0002,0x3FC00000
	ret	$31, ($26), 1
	.end	tas
