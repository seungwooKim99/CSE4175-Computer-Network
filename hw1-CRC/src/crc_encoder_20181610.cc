#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INVALID_INPUT 0
#define VALID_INPUT 1

#define UPPER_MASK 0xf0
#define LOWER_MASK 0x0f
#define FULL_MASK UPPER_MASK || LOWER_MASK

#define MAX_BYTES 30

/* macros */
#define SHIFT_LEFT(num, n) ((num) << (n))
#define SHIFT_RIGHT(num, n) ((num) >> (n))


/* global variables */
const unsigned char msb = 0b10000000;
const unsigned char lsb = 0b00000001;

FILE *input_fp;
FILE *output_fp;
int padding_bits_num = 0;
unsigned char generator[MAX_BYTES] = {0x00,};
size_t generator_size;
unsigned char codeword[MAX_BYTES] = {0x00,};
size_t dataword_size;
size_t codeword_size;

int input_file_size;
int coded_file_size;

// saved memory codewords
unsigned char *mem_codewords;
int mem_codewords_idx = 0;

/* test function */
void print_b(char num){
  for (int i = 7; i >= 0; --i) { //8자리 숫자까지 나타냄
        int result = num >> i & 1;
        printf("%d", result);
        if (i == 4) printf(" ");
    }
  printf("\n");
}

void set_bit(unsigned char *array, int i) {
    array[i / 8] |= (1 << (7 - (i % 8)));
}

unsigned char toBinary(int n) {
    /* 0 ~ 255 의 10진수 정수 n을 2진수로 변환 */
    unsigned char result = 0;
    int divisor = 128;
    for (int i = 0; i < 8; i++) {
        if (n >= divisor) {
            result |= (1 << (7 - i));
            n -= divisor;
        }
        divisor /= 2;
    }
    return result;
}


void parse_string_to_binary(char *str, unsigned char *binary, size_t *binary_size) {
  /* -------------------------------------------
   * char *str : 이진수 스트링 (ex. 1011011)
   * unsigned char *binary : 이진수를 담을 배열 포인터
   * size_t *binary_size : 이진수 길이를 담을 주소
   * -------------------------------------------
   * ex. 10110001 1001 str을 binary에 저장하면 다음과 같다.
   * binary[0] -> 0b10110001, binary[1] -> 0b10010000
   * (길이를 초과하는 나머지 bit는 0)
   */
  *binary_size = strlen(str);

  unsigned char bit = 0b10000000;
  int cnt = 0;
  while (cnt < *binary_size){
    if (cnt % 8 == 0) {
      bit = 0b10000000;
    }
    if (str[cnt] == '1') {
      binary[cnt/8] = binary[cnt/8] | bit;
    }
    bit = SHIFT_RIGHT(bit, 1);
    cnt++;
  }
  return;
}

int init(int argc, char **argv){
  /* validate */
  if (argc != 5) {
    printf("usage: ./crc_encoder input_file output_file generator dataword_size\n");
    return INVALID_INPUT;
  }
  if (!(input_fp = fopen(argv[1], "r"))){
    printf("input file open error.\n");
    return INVALID_INPUT;
  }
  if (!(output_fp = fopen(argv[2], "wb"))){
    printf("output file open error.\n");
    return INVALID_INPUT;
  }
  dataword_size = (size_t)atoi(argv[4]);
  if ((dataword_size != 4) && (dataword_size != 8)){
    printf("dataword size must be 4 or 8.\n");
    return INVALID_INPUT;
  }

  /* parse generator to binary (argv[3])*/
  parse_string_to_binary(argv[3], generator, &generator_size);
  codeword_size = dataword_size + generator_size - 1;

  return VALID_INPUT;
}

void shift_left(unsigned char *binary_arr, int num) {
  for (int i = 0 ; i < num ; i++){
    binary_arr[0] = SHIFT_LEFT(binary_arr[0],1);
    for(int j=1; j < MAX_BYTES ; j++) { // shift overflow 처리
        if ((binary_arr[j] & msb) == msb) {
          binary_arr[j-1] |= lsb;
        }
        binary_arr[j] = SHIFT_LEFT(binary_arr[j],1);
      }
  }
}

void shift_right(unsigned char *binary_arr, int num) {
  for (int i = 0 ; i<num ; i++){
    binary_arr[MAX_BYTES - 1] = SHIFT_RIGHT(binary_arr[MAX_BYTES - 1],1);
    for(int j=MAX_BYTES - 2; j >= 0 ; j--) { // shift overflow 처리
        if ((binary_arr[j] & lsb) == lsb) {
          binary_arr[j+1] |= msb;
        }
        binary_arr[j] = SHIFT_RIGHT(binary_arr[j],1);
      }
  }
}

void encode(unsigned char dataword){
  /* 초기화 */
  memset(codeword, 0x00, sizeof(codeword));
  codeword[0] |= dataword;

  /* divide */
  int shift_num = (int)(dataword_size);
  while (shift_num > 0) {
    if ((codeword[0] & msb) == msb) {
      for(int i = 0 ; i < MAX_BYTES ; i++) {
        codeword[i] = codeword[i]^generator[i];
      }
    }
    shift_left(codeword, 1);
    shift_num--;
  }

  /* add remainder to dataword */
  shift_right(codeword, dataword_size);
  codeword[0] |= dataword;
  return;
}

void calc_padding_bits_num(char **argv){
  fseek(input_fp, 0, SEEK_END);
  input_file_size = ftell(input_fp);
  fseek(input_fp, 0, 0);

  if (dataword_size == 4) {
      padding_bits_num = 8 - (2*codeword_size*input_file_size)%8;
  } else {
      padding_bits_num = 8 - (codeword_size*input_file_size)%8;
  }
  if (padding_bits_num == 8) {
    padding_bits_num = 0;
  }
  return;
}

int main(int argc, char **argv){
    char ch;
    unsigned char dataword;
    unsigned char saved_codeword[MAX_BYTES] = {0x00,};

    /* Initialize and validate input arguments */
    if (init(argc, argv) == INVALID_INPUT){
      return 0;
    }
    /* calc padding bits number with filesize */
    calc_padding_bits_num(argv);

    /* memory allocation on codeword buffer */
    if (dataword_size == 4){
        coded_file_size = (8 + padding_bits_num + 2*codeword_size*input_file_size)/8;
    } else {
        coded_file_size = (8 + padding_bits_num + codeword_size*input_file_size)/8;
    }
    mem_codewords = (unsigned char*)malloc(coded_file_size);

    /* calculate padding bits # */
    unsigned char padding_bits_num_binary = toBinary(padding_bits_num);
    mem_codewords[0] |= padding_bits_num_binary;
    mem_codewords_idx += 8;

    /* add padding bits */
    mem_codewords_idx += padding_bits_num;

    /* read character one-by-one and write encoded words */
    while ((ch = fgetc(input_fp)) != EOF) {
      memset(saved_codeword, 0x00, sizeof(saved_codeword));
      int i;
      int bits_to_write = 0;
      if (dataword_size == 4){
        /* upper dataword */
        dataword = (unsigned char)(ch & UPPER_MASK);
        encode(dataword);

        // save codeword
        for(i = 0 ; i < codeword_size ; i++) {
          if ((codeword[0] & msb) == msb){
            set_bit(mem_codewords, mem_codewords_idx);
          }
          shift_left(codeword, 1);
          mem_codewords_idx++;
        }

        /* lower dataword */
        dataword = (unsigned char)SHIFT_LEFT((ch & LOWER_MASK),4);
        encode(dataword);
        for(i = 0 ; i < codeword_size ; i++) {
          if ((codeword[0] & msb) == msb){
            set_bit(mem_codewords, mem_codewords_idx);
          }
          shift_left(codeword, 1);
          mem_codewords_idx++;
        }
      } else {
        // 1 dataword in 1 char
        dataword = (unsigned char)ch;
        encode(dataword);

        for(i = 0 ; i < codeword_size ; i++) {
          if ((codeword[0] & msb) == msb){
            set_bit(mem_codewords, mem_codewords_idx);
          }
          shift_left(codeword, 1);
          mem_codewords_idx++;
        }
      }
    }

    /* write to file*/
    fwrite(mem_codewords, sizeof(unsigned char), coded_file_size, output_fp);

    /* close file descriptors */
    fclose(input_fp);
    fclose(output_fp);

    return 0;
}