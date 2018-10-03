
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define STATELENGTH 36
#define KEYLENGTH   10
#define IVLENGTH    10

#define BUFFER_MAX_LENGTH 150
typedef uint8_t u8;
typedef long u64;

void ip_encrypt(u8* key, u8* iv, u8* input, u64 length);
void ip_decrypt(u8* key, u8* iv, u8* input, u64 length);

u8* encrypt(u8* key, u8* iv, u8* input, u64 length);
u8* decrypt(u8* key, u8* iv, u8* input, u64 length);

u8 trivum_state[STATELENGTH];
u8 trivium_output[64];


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


/***
 * setup
 *
 * setup the state per the key and iv
 *
 */
u8* setup(u8* key, u8* iv) {
  u8* mark = trivum_state;

  u8 zero = 0x00; // 00000000
  u8 end  = 0x07; // 00000111

  // the insert_* functions increment mark accordingly
  insert_key(&mark, key);
  insert_byte(&mark, &zero, 1);
  insert_iv(&mark, iv);
  insert_byte(&mark, &zero, 13);
  insert_byte(&mark, &end, 1);

  return trivum_state;
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
 * update
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

  for (bit = 8; bit > 0; bit--) {
    z = update(state);
    pb(&keystream, &z, (bit - 1));
  }

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

  reverse(key, KEYLENGTH);
  reverse(iv, IVLENGTH);

  u8* state;
  state = setup(key, iv);
  for (iter = 0; iter < (4 * 288); iter++) update(state);

  u8 keystream;
 
  for (; mark > 0; mark--) {
    keystream = stream(state);
    //printf("|%02x|",keystream);
    input[(length - mark)] ^= keystream;
  }
  return;
}


/***
 * cipher
 *
 * generate and apply keystream
 *
 */
u8* cipher(u8* key, u8* iv, u8* input, u64 length) {

  memmove(trivium_output, input, length);
  ip_cipher(key, iv, trivium_output, length);
 
  return trivium_output;
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
 * encrypt
 *
 * encrypt (syntactic sugar for cipher function)
 *
 */
u8* encrypt(u8* key, u8* iv, u8* input, u64 length) {
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


int main(int argc, char ** argv)
{

    //arrays and variables

    //space for the plain text
    unsigned char in[64]; 
    //space for the chiper text
    u8 out[64];  
    u8 key[10];
    u8 iv[10];
   	//space for keeping hexadecimal ASCII representation of an 8 bit number
   	char hex[2]; 
   	//space to read the ASCII characters coming through the serial in
   	char buffer[BUFFER_MAX_LENGTH]; 
	//space for holding one key at a time (20 characters)
	char key_set[25];
	char iv_set[25];

   	FILE *fp_in;    // input file
   	FILE *fp_out;   // output file
	FILE *fp_in_keys;  // input keys
   	FILE *fp_in_ivs;  // input ivs

   	//open regarded files
   	if(argc == 3){
   		fp_in = fopen (argv[1], "r");
   		fp_out = fopen (argv[2], "w");
		fp_in_keys = fopen("keys.txt","r");
		fp_in_ivs = fopen("ivs.txt","r");
   	}else 
   		printf("please enter correct arguments\n");

   	int i, count = 0;

   	if ((fp_in == NULL) || (fp_out == NULL) || (fp_in_keys == NULL))
   		printf(" could'nt find the input or output files\n");
	
	// get the plain text into buffer
	fgets(buffer, sizeof(buffer), fp_in);

   	while ((fgets(key_set, sizeof(key_set), fp_in_keys) != NULL) && (fgets(iv_set, sizeof(key_set), fp_in_ivs) != NULL)){
		// convert character keys to hexadecimals
		for(i=0;i<10;i++){
        		hex[0] = key_set[i*2];
        		hex[1] = key_set[i*2+1];
        		key[i] = convertdigit(hex[1])+16*convertdigit(hex[0]);
    		}

		// convert character ivs to hexadecimals
		for(i=0;i<10;i++){
        		hex[0] = iv_set[i*2];
        		hex[1] = iv_set[i*2+1];
        		iv[i] = convertdigit(hex[1])+16*convertdigit(hex[0]);
    		}
		
		// calling for the initial setup
	 	setup(key,iv);	

    		// convert the input string to a byte array	
    		for(i=0;i<64;i++){
        		hex[0] = buffer[i*2];
        		hex[1] = buffer[i*2+1];
        		in[i] = convertdigit(hex[1])+16*convertdigit(hex[0]);
    		}

    		memcpy(out, encrypt(key, iv, in, 64), 64);

    		for(i=0; i<20; i++) key_set[i] = 0;
                 
    		for (i=0;i<64;i++){
        		//printf("%02X", out[i] );
        		fprintf(fp_out, "%02X", out[i]);  // write to the output file
    		}

    		fprintf( fp_out, "\n");  // keep new lines
    		count++;
    }

    // close opened files
    fclose(fp_in);
    fclose(fp_out);
    fclose(fp_in_keys);
    printf(" (*)%d cipher texts are generated\n", count);
    return 0;
 
}



