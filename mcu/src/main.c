//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include "SI_C8051F850_Register_Enums.h"

volatile U8    l,c,r;
volatile U8    state;
volatile U16   sw_timer_seg;
volatile U16   sw_timer_rot;
volatile U16   i;
volatile U8    last_s0, last_s1;
volatile U8    state_rot;
   
void updateDisplay(U16 num);
void drive7seg(U8 num);

//-----------------------------------------------------------------------------
// Pin Declarations
//-----------------------------------------------------------------------------

SBIT(LTOP, SFR_P0, 2);
SBIT(LBOT, SFR_P0, 0);
SBIT(CTOP, SFR_P0, 1);
SBIT(CMID, SFR_P0, 5);
SBIT(CBOT, SFR_P1, 7);
SBIT(RTOP, SFR_P0, 6);
SBIT(RBOT, SFR_P0, 7);
SBIT(RDOT, SFR_P1, 0);

SBIT(LEFT, SFR_P0, 4);
SBIT(CNTR, SFR_P0, 3);
SBIT(RGHT, SFR_P1, 1);

SBIT(S0, SFR_P1, 5);  
SBIT(S1, SFR_P1, 6);                   

SBIT(NCS, SFR_P2, 1);                   

U16 spiTransfer (U16 data);
void setResistors(U8 one, U8 two);

//-----------------------------------------------------------------------------
// MAIN Routine
//-----------------------------------------------------------------------------
void main (void){
   U16 ratio; 
   U16 last_i;
   setup();                        // Initialize crossbar and GPIO

   i = 500; 
   
   setResistors(0,0);
   while (1){ 
     
      if(last_i != i){
         
          updateDisplay(i);
     
         ratio = (i/125) - 1;
         
         setResistors(0,(i & 0xFF));
      }
      last_i = i;
         
      
   };
}

void setResistors(U8 one, U8 two){
   U16 cmd;
   cmd = 0;
   cmd += one;
   spiTransfer(cmd);
   cmd = 0x1000;
   cmd += two;
   spiTransfer(cmd);

}

U16 spiTransfer (U16 data){
   U8 get;
   NCS = 0;
   SPI0DAT = (data >> 8) & 0xFF;;
   while(0 != (SPI0CFG & SPI0CFG_SPIBSY__BMASK));
   SPI0DAT = data & 0xFF;;
   while(0 != (SPI0CFG & SPI0CFG_SPIBSY__BMASK));
   NCS = 1;
   return get; 
}

INTERRUPT (TIMER2_ISR, TIMER2_IRQn){			   // Timer running at 4KHz   
   
   sw_timer_seg++;
   if(1000 == sw_timer_seg){
      sw_timer_seg = 0;
      switch(state){
         case 0:  RGHT = 0;
                  LEFT = 1;
                  drive7seg(l+10);
                  state = 1;
                  break;
         case 1:  LEFT = 0;
                  CNTR = 1;
                  drive7seg(c);
                  state = 2;
                  break;
         default: CNTR = 0;
                  RGHT = 1;
                  drive7seg(r);
                  state = 0;
                  break;
      } 
   }
   
   if(sw_timer_rot < 20000){
      sw_timer_rot++;
   }
   switch(state_rot){
      case 0:  if( (last_s0 == 1) && (S0 == 0))
                  state_rot = 2;
               if( (last_s1 == 1) && (S1 == 0))
                  state_rot = 1;
               break;
      case 1:  if( (last_s0 == 1) && (S0 == 0)){
                  state_rot = 0;
                  i += 20000/sw_timer_rot;
                  sw_timer_rot = 0;
               }
               if(S1 == 1)
                  state_rot = 0;
               break;
      case 2:  if( (last_s1 == 1) && (S1 == 0)){
                  state_rot = 0;
                  i -= 20000/sw_timer_rot;
                  sw_timer_rot = 0;
               }
               if(S0 == 1)
                  state_rot = 0;
               break;
      default: state_rot = 0;    
   } 
   last_s0 = S0;
   last_s1 = S1;
}

void setup(void){
   // Watchdog
   WDTCN = 0xDE;
   WDTCN = 0xAD;
   // Clock
	CLKSEL = CLKSEL_CLKSL__HFOSC 	      |  // Use 24.5MHz interal clock
			   CLKSEL_CLKDIV__SYSCLK_DIV_1;  // Do not divide
   // IO
   P0MDIN   = 0xFF;  // All output
   P0MDOUT  = 0xFF;
   P1MDIN   = 0xFF;
   P1MDOUT  = 0xFF; 
   P2MDOUT  = 0xFF; 
   P1MDOUT  &= ~0x60;   // P1.5 and P1.6 are inputs
   P1       |= 0x60;   
	P0SKIP   = 0xFF;
   P1SKIP   = 0xE3; // 2,3,4 used for SPI 
   XBR0     = XBR0_SPI0E__ENABLED;
   XBR2     = 0x40;  // Enable crossbar and weak pull-ups
   // SPI
   SPI0CFG  = SPI0CFG_MSTEN__MASTER_ENABLED; 
   SPI0CN   = SPI0CN_SPIEN__ENABLED;
   SPI0CKR  = 0xFF;
   // Timers
   TCON =   TCON_TR0__BMASK &	 
			   TCON_TR1__BMASK;
	TH1 =    (150 << TH1_TH1__SHIFT);
   TL1 =    (150 << TL1_TL1__SHIFT);
   CKCON =  CKCON_SCA__SYSCLK_DIV_12   | 
			   CKCON_T0M__PRESCALE 			| 
			   CKCON_T3MH__EXTERNAL_CLOCK | 
			   CKCON_T3ML__EXTERNAL_CLOCK	| 
			   CKCON_T1M__SYSCLK;
	TMOD =   TMOD_T0M__MODE0 				| 
			   TMOD_CT0__TIMER 				| 
			   TMOD_GATE0__DISABLED			| 
			   TMOD_T1M__MODE2 				| 
			   TMOD_CT1__TIMER 				| 
			   TMOD_GATE1__DISABLED;
	TCON |=  TCON_TR1__RUN; 
   TMR2CN = TMR2CN_TR2__RUN;
   // Interrupts
	IE =  IE_EA__ENABLED    | 
		   IE_EX0__DISABLED  | 
			IE_EX1__DISABLED  | 
			IE_ESPI0__DISABLED| 
			IE_ET0__DISABLED  | 
			IE_ET1__ENABLED   |
			IE_ET2__ENABLED   |
			IE_ES0__ENABLED;

}

void updateDisplay(U16 num){
   U16 d;
   l = (num / 100);
   d = num - (l*100);
   c = (d / 10);
   r = d - (c*10);
}

void drive7seg(U8 num){
   U8 n;
   if(num > 9){
      RDOT = 1;
      n = num -10;
   } else {
      RDOT = 0;
      n = num;
   }
   switch(n){
      case 0:     LTOP = 1; 
                  LBOT = 1;
                  CTOP = 1;
                  CMID = 0;
                  CBOT = 1;
                  RTOP = 1;
                  RBOT = 1; 
                  break;
      case 1:     LTOP = 0; 
                  LBOT = 0;
                  CTOP = 0;
                  CMID = 0;
                  CBOT = 0;
                  RTOP = 1;
                  RBOT = 1; 
                  break;
      case 2:     LTOP = 0; 
                  LBOT = 1;
                  CTOP = 1;
                  CMID = 1;
                  CBOT = 1;
                  RTOP = 1;
                  RBOT = 0; 
                  break;
      case 3:     LTOP = 0; 
                  LBOT = 0;
                  CTOP = 1;
                  CMID = 1;
                  CBOT = 1;
                  RTOP = 1;
                  RBOT = 1; 
                  break;
      case 4:     LTOP = 1; 
                  LBOT = 0;
                  CTOP = 0;
                  CMID = 1;
                  CBOT = 0;
                  RTOP = 1;
                  RBOT = 1; 
                  break;
      case 5:     LTOP = 1; 
                  LBOT = 0;
                  CTOP = 1;
                  CMID = 1;
                  CBOT = 1;
                  RTOP = 0;
                  RBOT = 1; 
                  break;
      case 6:     LTOP = 1; 
                  LBOT = 1;
                  CTOP = 1;
                  CMID = 1;
                  CBOT = 1;
                  RTOP = 0;
                  RBOT = 1; 
                  break;
      case 7:     LTOP = 0; 
                  LBOT = 0;
                  CTOP = 1;
                  CMID = 0;
                  CBOT = 0;
                  RTOP = 1;
                  RBOT = 1; 
                  break;
      case 8:     LTOP = 1; 
                  LBOT = 1;
                  CTOP = 1;
                  CMID = 1;
                  CBOT = 1;
                  RTOP = 1;
                  RBOT = 1; 
                  break;
      case 9:     LTOP = 1; 
                  LBOT = 0;
                  CTOP = 1;
                  CMID = 1;
                  CBOT = 0;
                  RTOP = 1;
                  RBOT = 1; 
                  break;
      default:    LTOP = 0; 
                  LBOT = 0;
                  CTOP = 0;
                  CMID = 0;
                  CBOT = 0;
                  RTOP = 0;
                  RBOT = 0;
                  break;
   }
    
}
