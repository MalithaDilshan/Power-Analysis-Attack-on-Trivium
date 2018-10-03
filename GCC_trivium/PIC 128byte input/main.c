// The header file for the microcontroller. Change this if your microcontroller is different
#include <18F2550.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define STATELENGTH 36
#define KEYLENGTH   10
#define IVLENGTH    10

typedef uint8_t u8;
typedef long u64;

void ip_encrypt(u8* key, u8* iv, u8* input, u64 length);
void ip_decrypt(u8* key, u8* iv, u8* input, u64 length);

u8* encrypt(u8* key, u8* iv, u8* input, u64 length);
u8* decrypt(u8* key, u8* iv, u8* input, u64 length);

/************************************************ DEVICE DEPENDENT CONFIGURATION *******************************************************/


//configurations bits. Note that these changes depending on the microcontroller
#fuses HSPLL,NOWDT,NOPROTECT,NOLVP,NODEBUG,PLL2,CPUDIV1,NOVREGEN,NOBROWNOUT,NOMCLR  
/*
HSPLL - High Speed Crystal/Resonator with PLL enabled. HSPLL requires the crystal to be >=4MHz
NOWDT - disable watch dog timer      
NOPROTECT - Code not protected from reading
NOLVP - No low voltage programming, BB5 used for I/O
NODEBUG - No Debug mode for ICD
PLL2 - Divide By 2(8MHz oscillator input). The input crystal frequency must be divided and brought to 4MHz to be fed to the PLL. PLL converts the 4MHz signal to 96MHz. Since our crustal is 8MHz we divide by 2 to bring it to 4MHz by specifying PLL2
CPUDIV1 - No System Clock Postscaler.
NOVREGEN - Internal voltage regulator disabled
NOBROWNOUT - No brownout reset
NOMCLR - No master clear reset
*/
//configuration is such that a 8MHz crystal input is converted to operate at 48MHz

//the effective clock frequency (48MHz) to be used for things like serial port communication, sleep etc
#use delay(clock=48000000)
//settings for the UART
#use rs232(UART1,baud=9600,parity=N,bits=8)


/*************
 * Utilities *
 *************/



/***
 * scw (subsequent crosswrite)
 *
 * write the last n bits of a byte into the first n bits of the next
 *
 */
void scw(u8* split, u64 offset) {

  u8* post = split;
  u8* pre  = split - 1;

  // e.g., for 3 -> 11111111 shifted to 11100000
  u8 mask = 0xFF << (8 - offset);

  // e.g., for 3 -> 00000101 shifted to 10100000
  u8 shifted = (*pre) << (8 - offset);

  (*post) = (*post & ~mask) | (shifted & mask);
  (*pre) >>= offset;

  return;
}


/***
 * acw (antecedent crosswrite)
 *
 * write the first n bits of a byte into the last n bits of the previous
 *
 */
void acw(u8* split, u64 offset) {

  u8* post = split;
  u8* pre  = split - 1;

  // e.g., for 3 -> 00000001 shifted to 00001000 becomes 00000111
  u8 mask = (0x01 << offset) - 1;

  // e.g., for 3 -> 10100000 shifted to 00000101
  u8 shifted = (*post) >> (8 - offset);

  (*pre) = (*pre & ~mask) | (shifted & mask);
  (*post) <<= offset;

  return;
}



/*****************
 * Initialization *
 *****************/



/***
 * insert_byte
 *
 * insert a byte a repeat number of times
 *
 */
void insert_byte(u8** p_mark, u8* b8, u64 repeat) {

  u8* mark = (*p_mark);
  u64 iter;

  for (iter = 0; iter < repeat; iter++) {
    mark = memmove(mark, b8, 1);
    mark += 1;
  }

  (*p_mark) = mark;

  return;
}


/***
 * insert_key
 *
 * insert the key into the state
 *
 */
void insert_key(u8** p_mark, u8* key) {

  u8* mark = (*p_mark);

  memmove(mark, key, KEYLENGTH);
  mark += KEYLENGTH;

  (*p_mark) = mark;

  return;
}


/***
 * insert_iv
 *
 * insert the iv - this requires cross writing
 * across bytes, since the iv is not positioned
 * on a clean byte boundary in the state
 *
 */
void insert_iv(u8** p_mark, u8* iv) {

 // u8* temp;
  u64 iter;
  u8* mark = (*p_mark);

  // zero out the current byte - we'll acw
  // the first three bits of the iv into it.
  (*mark) = 0x00;
  mark += 1;

  for (iter = 0; iter < IVLENGTH; iter++) {
    memmove(mark, iv + iter, 1);
    acw(mark, 3);

    mark += 1;
  }

  (*p_mark) = mark;

  return;
}

u8 stateee[STATELENGTH];

/***
 * setup
 *
 * setup the state per the key and iv
 *
 */
u8* setup(u8* key, u8* iv) {
  //u8* stateee;

  u8* mark = stateee;

  u8 zero = 0x00; // 00000000
  u8 end  = 0x07; // 00000111

  // the insert_* functions increment mark accordingly

  insert_key(&mark, key);

  insert_byte(&mark, &zero, 1);

  insert_iv(&mark, iv);

  insert_byte(&mark, &zero, 13);
  insert_byte(&mark, &end, 1);

  return stateee;
}



/************************
 * Keystream Generation *
 ************************/



/***
 * gb
 *
 * get the bit at a given index in a byte
 *
 */
u8 gb(u8* from, u64 index) {

  u8 b8 = (*from);

  u8 shifted = b8 >> (7 - index);

  return shifted & 0x01;
}


/***
 * pb
 *
 * put the bit at a given index in a byte
 *
 */
void pb(u8* to, u8* from, u64 index) {

  u8 mask = (0x01 << (7 - index));

  u8 put = (*from);
  put <<= (7 - index);

  (*to) = (*to & ~mask) | (put & mask);

  return;
}


/***
 * gsb
 *
 * get the bit at a given index in the state
 *
 */
u8 gsb(u8* state, u64 index) {

  u64 b8 = index / 8;
  u64 bit  = index % 8;

  return gb(state + b8, bit);
}


/***
 * psb
 *
 * put the bit at a given index in the state
 *
 */
void psb(u8* state, u8* from, u64 index) {

  u64 b8 = index / 8;
  u64 bit  = index % 8;

  pb(state + b8, from, bit);

  return;
}


/***
 * 
 *
 * generate a keystream bit and update the state accordingly
 *
 */
u8 update(u8* state) {
  u8 t1, t2, t3, z;
  u8 _ = 0x00;
  u64 iter;

  // indexes are from zero
  t1 = gsb(state, 65)  ^ gsb(state, 92);
  t2 = gsb(state, 161) ^ gsb(state, 176);
  t3 = gsb(state, 242) ^ gsb(state, 287);
   
  
  z = t1 ^ t2 ^ t3;
  
  t1 = t1 ^ (gsb(state, 90)  & gsb(state, 91))  ^ gsb(state, 170);
  t2 = t2 ^ (gsb(state, 174) & gsb(state, 175)) ^ gsb(state, 263);
  t3 = t3 ^ (gsb(state, 285) & gsb(state, 286)) ^ gsb(state, 68);

  // zero out the last bit so that state material is not
  // written into out of bounds memory when we shift up
  psb(state, &_, 287);

  // rotate
  for (iter = STATELENGTH; iter > 0; iter--) scw(state + iter, 1);

  // update
  psb(state, &t3, 0);
  psb(state, &t1, 93);
  psb(state, &t2, 177);

  return z;
}


/***
 * stream
 *
 * generate a keystream byte
 *
 */
u8 stream(u8* state) {

  u8 keystream, z;
  u64 bit;

  output_high (PIN_B0);
  for (bit = 8; bit > 0; bit--) {
    z = update(state);
    pb(&keystream, &z, (bit - 1));
  }
  output_low (PIN_B0);

  return keystream;
}



/**************
 * Cipherment *
 **************/



/***
 * reverse
 *
 * reverse input
 *
 */
void reverse(u8* str, u64 length) {

  u64 l = 0, r = length - 1;
  u8 t1, t2;

  for (; l < r; l++, r--) {
    t1 = str[l];
    t2 = str[r];

    str[l] = t2;
    str[r] = t1;
  }

  return;
}


/***
 * ip_cipher
 *
 * generate and apply keystream on input in place
 *
 */
void ip_cipher(u8* key, u8* iv, u8* input, u64 length) {

  u64 mark = length;
  u64 iter;
  u8 input_backup [64];
  memcpy(input_backup,input,64);
  //reverse(key, KEYLENGTH);
  //reverse(iv, IVLENGTH);

  u8* state;
  state = setup(key, iv);
  for (iter = 0; iter < (4 * 288); iter++){;update(state);}
  u8 keystream;
   
  //printf("\n___________keystream__________\n");
  output_high (PIN_B0);
  for (; mark > 0; mark--) {
    keystream = stream(state);
    //printf("|%02x|XOR|%02x|",keystream, input_backup[(length - mark)]);   
    input_backup[(length - mark)] ^= keystream;
  }
  output_low (PIN_B0);
  memcpy(input,input_backup,64);
  return;
}

u8 outputtt[64];

/***
 * cipher
 *
 * generate and apply keystream
 *
 */
u8* cipher(u8* key, u8* iv, u8* input, u64 length) {

  //u8* outputtt;

  memmove(outputtt, input, length);
  ip_cipher(key, iv, outputtt, length);

  return outputtt;
}



/********
 * APIs *
 ********/



/***
 * ip_encrypt
 *
 * encrypt in place (syntactic sugar for in place cipher function)
 *
 */
void ip_encrypt(u8* key, u8* iv, u8* input, u64 length) {
  ip_cipher(key, iv, input, length);

  return;
}


/***
 * ip_decrypt
 *
 * decrypt in place (syntactic sugar for in place cipher function)
 *
 */
void ip_decrypt(u8* key, u8* iv, u8* input, u64 length) {
  ip_cipher(key, iv, input, length);

  return;
}


/***
 * encrypt
 *
 * encrypt (syntactic sugar for cipher function)
 *
 */
u8* encrypt(u8* key, u8* iv, u8* input, u64 length) {
  return cipher(key, iv, input, length);
}


/***
 * decrypt
 *
 * decrypt (syntactic sugar for cipher function)
 *
 */
u8* decrypt(u8* key, u8* iv, u8* input, u64 length) {
  return cipher(key, iv, input, length);
}

int convertdigit(char digit){
   
   unsigned char value=-1;
   switch (digit){
   
   case '0':
      value=0;
      break;
   case '1':
      value=1;
      break;
   case '2':
      value=2;
      break;
   case '3':
      value=3;
      break;
   case '4':
      value=4;
      break;
   case '5':
      value=5;
      break;
   case '6':
      value=6;
      break;
   case '7':
      value=7;
      break;      
   case '8':
      value=8;
      break;
   case '9':
      value=9;
      break;
   case 'A':
      value=10;
      break;
   case 'B':
      value=11;
      break;   
   case 'C':
      value=12;
      break;
   case 'D':
      value=13;
      break;
   case 'E':
      value=14;
      break;
   case 'F':
      value=15;
      break;   
   }

   return value;
}
//Main program

void main()
{

   //arrays and variables 
   unsigned char in[64];   //space the plain text
   u8 out[64];  //space for the cipher text
   u8 key[10];
   u8 iv[10];
   char plain_text_buffer[129] = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
"; 
   char buffer[45];  //space to read the ASCII characters coming through the serial in
   char hex[2]; //space for keeping hexadecimal ASCII representation of an 8 bit number
   int i;

   call_setup = 1;
   
   //convert the input string to a byte array
   for(i=0;i<64;i++){
   	hex[0]=plain_text_buffer[i*2];
   	hex[1]=plain_text_bufferbuffer[i*2+1];
   	in[i]=convertdigit(hex[1])+16*convertdigit(hex[0]);
   }
   
   //infinitely take plain text, encrypt and send cipher text back    
   while(1){
 
         //get the input character string to buffer. Since a plain text block is 128 bits it is 32 characters
         for (i=0;i<40;i++){
            buffer[i]=getc();
         
         //some error correction mechanism. If the host sends a 'y' some issue has occurred, clean all the things in the buffer
            if(buffer[i]=='y'){
               while(kbhit()){
                    temp=getc();
               }
            }
         
         }
         buffer[i]=0; //terminating character
         
         //convert the input string to a byte array
         for(i=0;i<10;i++){
            hex[0]=buffer[i*2];
            hex[1]=buffer[i*2+1];
            key[i]=convertdigit(hex[1])+16*convertdigit(hex[0]);
         }

	 //convert the input string to a byte array
         for(i=10;i<20;i++){
            hex[0]=buffer[i*2];
            hex[1]=buffer[i*2+1];
            iv[i]=convertdigit(hex[1])+16*convertdigit(hex[0]);
         }
	
	 if(call_setup == 1){
	 	//set the key   
   		setup(key,iv); 
		call_setup=0;
         }
     

         //prints the plain text via the serial port. The computer can check if communication happen properly
         for (i=0;i<64;i++){
               printf("%2X", in[i] );
         }
         
         //We need to repeatedly do the encryption on the plain text sample until the host computer aquires the power trace via the oscilloscope
         //hence repeatedly do the encryption until host sends a signal to stop so
         while(1){
         
         //if the host computer has sent a signal, get it and behave appropriately
            if(kbhit()){

               temp=getc();
               
            //if the host sends 'z' thats the stopping signal and hence stop encryption and get ready to goto next round
               if(temp=='z'){
                  break;
               }
            
               //if something other than 'z' is received clean everything in the buffers and get ready for the next round
               else{
                  while(kbhit()){
                     temp=getc();
                  }
                  break;
               }
            }
         
               //if the host computer has sent no signal, repeatedly do the encryption
            else{            
               //encryption
               memcpy(out, encrypt(key, iv, in, 64), 64);
            }
         }
         /*
         memcpy(out, encrypt(key, iv, in, 64), 64);
         delay_ms(5);
         */
         //prints the cipher text to verify by the host whether cryptosystem is ing properly
         for (i=0;i<64;i++){
               printf("%2X", out[i] );
         }
         
         //just keep a delay
         delay_ms(5);
 
   }
}



