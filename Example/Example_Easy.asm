;
; LightSpeedPlayer usage example
; 

		
		code
		
			move.w	#(1<<5)|(1<<6)|(1<<7)|(1<<8),$dff096

			bsr		clearSprites

			move.w	#$0,$dff1fc
			move.w	#$200,$dff100	; 0 bitplan
			move.w	#$04f,$dff180
	
		; Init LSP and start replay using easy CIA toolbox
			lea		LSPMusic,a0
			lea		LSPBank,a1
			suba.l	a2,a2			; suppose VBR=0 ( A500 )
			moveq	#0,d0			; suppose PAL machine
			bsr		LSP_MusicDriver_CIA_Start

			move.w	#$e000,$dff09a

mainLoop:	bra.s	mainLoop


		; Include simple CIA toolkit
			include	"..\LightSpeedPlayer_cia.asm"

		; Include generic LSP player
			include	"..\LightSpeedPlayer.asm"
		
clearSprites:
			lea		$dff140,a0
			moveq	#8-1,d0			; 8 sprites to clear
			moveq	#0,d1
.clspr:		move.l	d1,(a0)+
			move.l	d1,(a0)+
			dbf		d0,.clspr
			rts

		data_c

LSPBank:	incbin	"rink-a-dink.lsbank"
			even

		data

LSPMusic:	incbin	"rink-a-dink.lsmusic"
			even
