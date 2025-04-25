;*****************************************************************
;
;	Light Speed Player v1.26
;	Fastest Amiga MOD player ever :)
;	Written By Arnaud Carré (aka Leonard / OXYGENE)
;	https://github.com/arnaud-carre/LSPlayer
;	twitter: @leonard_coder
;
;	"cia" player version ( or "less effort" )
;
;	Warnings:
;	a)	this file is provided for "easy of use". But if you're working
;		on a cycle-optimizated demo effect, please call LightSpeedPlayer from your
;		own existing interrupt and use copper to set DMACON 11 raster lines later
;
;	b)	this code doesn't restore any amiga OS stuff.
;		( are you a cycle-optimizer or what? :) )
;
;	--------How to use--------- 
;
;	bsr LSP_MusicDriver_CIA_Start : Init LSP player code and install CIA interrupt
;		a0: LSP music data(any memory)
;		a1: LSP sound bank(chip memory)
;		a2: VBR (CPU Vector Base Register) ( use 0 if 68000 )
;		d0: 0=PAL, 1=NTSC
;
;	bsr LSP_MusicDriver_CIA_Stop : Stop LSP music replay
;
;*****************************************************************
LSP_MusicDriver_CIA_Start:
			move.w	d0,-(a7)
			lea		.irqVector(pc),a3
			lea		$78(a2),a2
			move.l	a2,(a3)
			lea		.LSPDmaCon+1(pc),a2		; DMACON byte patch address
			bsr		LSP_MusicInit			; init the LSP player ( whatever fast or insane version )

			lea		.pMusicBPM(pc),a2
			move.l	a0,(a2)					; store music BPM pointer
			move.w	(a0),d0					; start BPM
			lea		.curBpm(pc),a2
			move.w	d0,(a2)
			moveq	#1,d1
			and.w	(a7)+,d1
			bsr.s	.LSP_IrqInstall

			rts

.LSPDmaCon:	dc.w	$8000
.irqVector:	dc.l	0
.ciaClock:	dc.l	0
.curBpm:	dc.w	0
.pMusicBPM:	dc.l	0

; d0: music BPM
; d1: PAL(0) or NTSC(1)
.LSP_IrqInstall:
			move.w 	#(1<<13),$dff09a		; disable CIA interrupt
			lea		.LSP_MainIrq(pc),a0
			move.l	.irqVector(pc),a5
			move.l	a0,(a5)

			lea		$bfd000,a0
			move.b 	#$7f,$d00(a0)
			move.b 	#$10,$e00(a0)
			move.b 	#$10,$f00(a0)
			lsl.w	#2,d1
			move.l	.palClocks(pc,d1.w),d1				; PAL or NTSC clock
			lea		.ciaClock(pc),a5
			move.l	d1,(a5)
			divu.w	d0,d1
			move.b	d1,$400(a0)
			lsr.w 	#8,d1
			move.b	d1,$500(a0)
			move.b	#$83,$d00(a0)
			move.b	#$11,$e00(a0)
			
			move.b	#496&255,$600(a0)		; set timer b to 496 ( to set DMACON )
			move.b	#496>>8,$700(a0)

			move.w 	#(1<<13),$dff09c		; clear any req CIA
			move.w 	#$a000,$dff09a			; CIA interrupt enabled
			rts
		
.palClocks:	dc.l	1773447,1789773

.LSP_MainIrq:
			btst.b	#0,$bfdd00
			beq.s	.skipa
			
			movem.l	d0-a6,-(a7)

		; call player tick
			lea		$dff0a0,a6
			bsr		LSP_MusicPlayTick		; LSP main music driver tick

		; check if BMP changed in the middle of the music
			move.l	.pMusicBPM(pc),a0
			move.w	(a0),d0					; current music BPM
			cmp.w	.curBpm(pc),d0
			beq.s	.noChg
			lea		.curBpm(pc),a2			
			move.w	d0,(a2)					; current BPM
			move.l	.ciaClock(pc),d1
			divu.w	d0,d1
			move.b	d1,$bfd400
			lsr.w 	#8,d1
			move.b	d1,$bfd500			

.noChg:		lea		.LSP_DmaconIrq(pc),a0
			move.l	.irqVector(pc),a1
			move.l	a0,(a1)
			move.b	#$19,$bfdf00			; start timerB, one shot

			movem.l	(a7)+,d0-a6
.skipa:		move.w	#$2000,$dff09c
			nop
			rte

.LSP_DmaconIrq:
			btst.b	#1,$bfdd00
			beq.s	.skipb
			move.w	.LSPDmaCon(pc),$dff096
			pea		(a0)
			move.l	.irqVector(pc),a0
			pea		.LSP_MainIrq(pc)
			move.l	(a7)+,(a0)
			move.l	(a7)+,a0
.skipb:		move.w	#$2000,$dff09c
			nop
			rte

LSP_MusicDriver_CIA_Stop:
			move.b	#$7f,$bfdd00
			move.w	#$2000,$dff09a
			move.w	#$2000,$dff09c
			move.w	#$000f,$dff096
			rts
