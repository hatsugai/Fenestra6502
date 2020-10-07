COMMAND = 0xFFF0
CMD_ARG = 0xFFF1

CMD_EXIT  = 0
CMD_GETCH = 1
CMD_PUTCH = 2

PTR        = 0xC0

	.r65c02						
    .area CODE (ABS)
    .org 0xFF00
start:
	ldx #0xFF
	txs
	;; msg
	lda #<msg
	ldx #>msg
	jsr putstr

	;; echo
loop:	
	jsr getch
	cmp #0x1F
	beq exit
	jsr putch
	bra loop

exit:	
	lda #CMD_EXIT
	sta COMMAND
	.byte 0xCB					; WAI

putstr:	
	sta *PTR
	stx *(PTR+1)
	ldy #0
putstr_loop:
	lda [PTR],y
	beq putstr_done
	jsr putch
	iny
	bra putstr_loop
putstr_done:	
	rts

getch:	
	lda #CMD_GETCH
	sta COMMAND
	.byte 0xCB					; WAI
	lda CMD_ARG
	rts

putch:	
	sta CMD_ARG
	lda #CMD_PUTCH
	sta COMMAND
	.byte 0xCB					; WAI
	rts

msg:	
	.byte 0x0D
	.asciz /Test6502/
