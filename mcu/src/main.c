//-----------------------------------------------------------------------------
// Project: psu
// File: 	main.c
// Brief:	Single main file containing all code, SI headers
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include "SI_C8051F850_Register_Enums.h"
#include "SI_C8051F850_Defs.h"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------

#define UART_SIZE_OUT   30

SBIT(LED0, SFR_P1, 0);  
SBIT(LED1, SFR_P1, 1);  
SBIT(BUT0, SFR_P1, 7);  
SBIT(BUT1, SFR_P2, 1);  

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------

U16 readAdc(void);
void ulo(U8 tx);
void display(U8 on_not_off, U16 mV, U16 mA);
void u16_to_dec_str(U16 data, U8 * ptr);
void character(U8 c, U8 line);


//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------
volatile U8 	uart_out[UART_SIZE_OUT];
volatile U8 	out_head;
volatile U8 	out_tail;

volatile U8    button;
volatile U8    psu_on;
volatile U32   counter;

//-----------------------------------------------------------------------------
// Main Routine
//-----------------------------------------------------------------------------

void main (void){
   // Init 
   U16 mA;
   U16 mV;
   U16 adc;
   U16 last_adc;
   U8  last_psu_on;
  
   counter     = 0;
   psu_on      = 0;
   last_psu_on = 0;

   out_head = 0;
   out_tail = 0;
   
   mA = 0;
   mV = 0;
   // !Init

   // Setup
   
   // Disabled watchdog
   WDTCN    = 0xDE;
   WDTCN    = 0xAD;
	
   // CPU clock
   CLKSEL   = CLKSEL_CLKSL__HFOSC | 
              CLKSEL_CLKDIV__SYSCLK_DIV_1;	 
  
   // Setup XBAR   
   P1MDIN   = P1MDIN_B2__ANALOG|                // ADC
              P1MDIN_B7__DIGITAL;               // Button P1.7
                                                // Button P2.1
   P0MDOUT  = P0MDOUT_B2__PUSH_PULL|             
              P0MDOUT_B4__PUSH_PULL;             
   P0SKIP   = 0xEB;                             // Do not skip P0.2 
                                                // Do not skip P0.4
   XBR1     = XBR1_PCA0ME__CEX0;                // Route out PCA0.CEX0 on P0.2
   XBR0     = XBR0_URT0E__ENABLED;              // Route out UART P0.4 
   XBR2     = XBR2_WEAKPUD__PULL_UPS_ENABLED | 
              XBR2_XBARE__ENABLED;					 
      
   // Setup 230400 Baud UART and BAUD gen on timer 1
	CKCON    = CKCON_T1M__SYSCLK;
	TMOD     = TMOD_T1M__MODE2;
	TCON     = TCON_TR1__RUN; 
   TH1      = 0xCB;                             // Magic values from datasheet
	TL1      = 0xCB;

   // Start 95.7KHz clock
   PCA0CN   = PCA0CN_CR__STOP;
   PCA0CPH0 = 0x00;
   PCA0CPL0 = 0x00;
   PCA0MD   = PCA0MD_CPS__SYSCLK;		 
   PCA0CPM0 = PCA0CPM0_ECOM__ENABLED  |  
              PCA0CPM0_MAT__ENABLED   | 
              PCA0CPM0_PWM__ENABLED;
   PCA0CN   = PCA0CN_CR__RUN;  
    
   // Timer 2
	TMR2CN   = TMR2CN_TR2__RUN;
  
   // ADC
   ADC0MX   = ADC0MX_ADC0MX__ADC0P10; 
   ADC0CN0  = ADC0CN0_ADEN__ENABLED; 
   
   // Interrupt
	IE       = IE_EA__ENABLED | 
			     IE_ET2__ENABLED;
   
   // !Setup  
   counter  = 0;
   LED0     = 1;  
     
   while(1){   
     
      // Sample button
      if((0 == BUT0) || (0 == BUT1))
         button = 1;  
      
      // Sample ADC
      adc = readAdc();   
      PCA0CPH0 = adc >> 2;
      mV = adc;

      //if(psu_on)
      //   mV = adc;
      //else
      //   mA = adc;
     

      // TODO:  
      // - PID controller

      // Update display 
      if((last_psu_on != psu_on) || ((last_adc >> 4) != (adc >> 4))){ 
         display(psu_on, mA, mV);
         last_psu_on = psu_on;
         last_adc    = adc;
      }


   }
} 

//-----------------------------------------------------------------------------
// Interrupt Routines
//-----------------------------------------------------------------------------

INTERRUPT (TIMER2_ISR, TIMER2_IRQn){      
   // Button timer
   if(counter == 0){
      if(button == 1){ 
         psu_on   = (psu_on + 1) % 2;  
         counter  = 2000;
      }
   }else{
      counter--;
   }
   button = 0;
      
   // UART
   if(out_head != out_tail){
      SBUF0 = uart_out[out_tail];            // Timer tuned so no need to check
      out_tail++;                            // Transmit UART
      out_tail %= UART_SIZE_OUT;             // Wrap around
   }
   
   // Timer
   TMR2H = 253;                              // Reset timer
   TMR2L = 240;                              // Tuned for baud rate
   TMR2CN_TF2H = 0; 
}

//-----------------------------------------------------------------------------
// Routines
//-----------------------------------------------------------------------------

U16 readAdc(void){
   ADC0CN0 |= ADC0CN0_ADBUSY__SET;
   while(ADC0CN0 & ADC0CN0_ADBUSY__SET);     // Wait for sample to complete
   return ADC0;        
}

void ulo(U8 tx){
   U8 new_head;
   new_head = (out_head + 1) % UART_SIZE_OUT;   
   while(new_head == out_tail);                 // Block until space in buffer
   uart_out[out_head] = tx;
	out_head = new_head;   
}

void display(U8 on_not_off, U16 mV, U16 mA){
   U8    i, j; 
   U8    num_str[5]; 
   U16   t[2];
   U8    v[2];
   t[0] = mV;
   t[1] = mA; 
   v[0] = 'V';
   v[1] = 'A';
   ulo(0xC);                        // Clear page 
   for(i=0;i<2;i++){ 
      u16_to_dec_str(t[i], num_str);  
      for(j=0;j<5;j++){
         ulo('=');
         ulo(' ');
         if((3 == j) && (i == on_not_off)){
            if(0 == on_not_off){
               ulo('O');
               ulo('F');
               ulo('F');
            }else{
               ulo('O');
               ulo('N');
               ulo(' ');
            }
         }else{
            ulo(' ');
            ulo(' ');
            ulo(' ');
         } 
         ulo(' ');
         ulo(' ');
         ulo(' ');
         character(num_str[0],j); 
         character(num_str[1],j); 
         character('.',j); 
         character(num_str[2],j); 
         character(num_str[3],j); 
         character(num_str[4],j); 
         character(v[i],j); 
         ulo('=');
         ulo('\n');
         ulo('\r');
      }
   } 
}

// Turn 16 bit data in to a string of 5 chars
void u16_to_dec_str(U16 data, U8 * ptr){
   U8 * p;
   S8 i,j;
   U16 c, d, m;
   d = data;
   p = ptr;
   for(i=4;i>-1;i--){ 
      m = 1;
      for(j=0;j<i;j++){
         m *= 10;
      }
      c = d / m; 
      *p = (U8)c + 48;
      *p++;
      d -= c * m;
   }
}

// Supports 0,1,2,3,4,5,6,7,8,9,A,V and .
void character(U8 c, U8 l){ 
   U8 m[6];
   U8 i; 
   m[0] = ' ';               
   m[1] = ' ';               
   m[2] = ' ';               
   m[3] = ' ';               
   m[4] = ' ';
   m[5] = ' ';                              
   if(   ((c == '0') && ((l == 0) || (l == 4)))                      || 
         ((c == '2') && ((l == 0) || (l == 2) || (l == 4)))          ||
         ((c == '3') && ((l == 0) || (l == 2) || (l == 4)))          ||
         ((c == '4') && (l == 3))                                    ||
         ((c == '5') && ((l == 0) || (l == 2) || (l == 4)))          ||
         ((c == '6') && ((l == 0) || (l == 2) || (l == 4)))          ||
         ((c == '7') && ((l == 0)))                                  ||
         ((c == '8') && ((l == 0) || (l == 2) || (l == 4)))          ||
         ((c == '9') && ((l == 0) || (l == 2) || (l == 4)))          ||
         ((c == 'A') && ((l == 0) || (l == 2)))                      ){
         m[1] = '-';               
         m[2] = '-';               
         m[3] = '-';               
         m[4] = '-';               
   }
   else if(   ((c == '1') && ((l == 0)))                             ){
         m[3] = '-';
   }
   else if(   ((c == '1') && ((l == 1) || (l == 2) || (l == 3)))     ){
         m[4] = '|'; 
   }
   else if(   ((c == '1') && ((l == 4)))                             ){
         m[3] = '-';
         m[4] = '-';
         m[5] = '-'; 
   }
   else if(   ((c == '2') && ((l == 3)))                             || 
         ((c == '5') && ((l == 1)))                                  || 
         ((c == '6') && ((l == 1)))                                  ){
         m[0] = '|';                   
   } 
   else if(   ((c == '2') && ((l == 1)))                             ||
         ((c == '3') && ((l == 1) || (l == 3)))                      ||
         ((c == '4') && (l == 4))                                    ||
         ((c == '5') && (l == 3))                                    ||
         ((c == '9') && (l == 3))                                    ){
         m[5] = '|';                   
   } 
   else if(   ((c == '0') && ((l == 1) || (l == 2) || (l == 3)))     ||
         ((c == '4') && ((l == 1) || (l == 2)))                      ||
         ((c == '6') && ((l == 3)))                                  ||
         ((c == '8') && ((l == 1) || (l == 3)))                      ||
         ((c == '9') && ((l == 1)))                                  ||
         ((c == 'A') && ((l == 1) || (l == 3)))                      ){
         m[0] = '|';               
         m[5] = '|';                
   }
   else if(   ((c == '7') && ((l == 1)))                             ){
         m[4] = '|'; 
   }
   else if(   ((c == '7') && ((l == 2)))                             ){
         m[3] = '|'; 
   }
   else if(   ((c == '7') && ((l == 3)))                             ){
         m[2] = '|'; 
   }
   else if(   ((c == '.') && ((l == 4)))                             ){
         m[3] = 'O'; 
   }
   else if(   ((c == 'V') && ((l == 1)))                             ){
         m[0] = '|';
         m[5] = '|'; 
   }
   else if(   ((c == 'V') && ((l == 2)))                             ){
         m[1] = '|';
         m[4] = '|'; 
   }
   else if(   ((c == 'V') && ((l == 3)))                             ){
         m[2] = '|';
         m[3] = '|'; 
   }
   for(i=0;i<6;i++)
      ulo(m[i]);
   ulo(' ');
   return;
}

//-----------------------------------------------------------------------------
// END
//-----------------------------------------------------------------------------
