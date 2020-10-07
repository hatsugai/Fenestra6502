COMMAND = 0xFFF0
CMD_ARG = 0xFFF1

CMD_EXIT  = 0
CMD_GETCH = 1
CMD_PUTCH = 2

START_ADDR = 0xC0
SIZE       = 0xC2
PTR        = 0xC4

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
	;; start address
	jsr getch
	sta *START_ADDR
	sta *PTR
	jsr getch
	sta *(START_ADDR+1)
	sta *(PTR+1)
	;; size
	jsr getch
	sta *SIZE
	jsr getch
	sta *(SIZE+1)
	;; program body
setup1:	
	lda *(SIZE+1)
	beq setup2
	dec a
	sta *(SIZE+1)
	ldy #0
loop1:	
	jsr getch
	sta [PTR],y
	iny
	bne loop1
	lda *(PTR+1)
	inc a
	sta *(PTR+1)
	bra setup1

setup2:	
	ldy #0
	ldx *SIZE
	beq exec
loop2:	
	jsr getch
	sta [PTR],y
	iny
	dex
	bne loop2
exec:	
	jmp [START_ADDR]

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
	.byte 0x0d
	.ascii /Fenestra6502 bootloader/
	.byte 0x0D
	.byte 0
