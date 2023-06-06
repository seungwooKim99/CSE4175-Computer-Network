#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INVALID_INPUT 0
#define VALID_INPUT 1

#define UPPER_MASK 0xf0
#define LOWER_MASK 0x0f

#define MAX_BYTES 30

/* macros */
#define SHIFT_LEFT(num, n) ((num) << (n))
#define SHIFT_RIGHT(num, n) ((num) >> (n))

/* global variables */
const unsigned char msb = 0b10000000;
const unsigned char lsb = 0b00000001;
int codeword_num = 0, error_num = 0;

FILE *input_fp;
FILE *output_fp;
FILE *result_fp;

unsigned char generator[MAX_BYTES] = {0x00,};
size_t generator_size;

unsigned char dataword[MAX_BYTES] = {0x00,};

unsigned char *mem_codewords;
int mem_codewords_idx = 0;
int input_file_size;

size_t dataword_size;
size_t codeword_size;

/* macros */
#define SHIFT_LEFT(num, n) ((num) << (n))
#define SHIFT_RIGHT(num, n) ((num) >> (n))

/* test function */
void print_b(char num){
  for (int i = 7; i >= 0; --i) { //8자리 숫자까지 나타냄
        int result = num >> i & 1;
        printf("%d", result);
        if (i == 4) printf(" ");
    }
  printf("\n");
}

int binaryToInt(unsigned char binary) {
    /* 8bit 2진수를 10진수 integer로 변환 */
    int result = 0;
    int base = 1;
    for(int i = 0 ; i < 8 ; i++) {
        if ((binary & lsb) == lsb) {
          result += base;
        }
        binary = SHIFT_RIGHT(binary, 1);
        base = SHIFT_LEFT(base, 1);
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

int init(int argc, char **argv) {
  /* validate */
  if (argc != 6) {
    printf("usage: ./crc_decoder input_file output_file result_file generator dataword_size\n");
    return INVALID_INPUT;
  }
  if (!(input_fp = fopen(argv[1], "rb"))){
    printf("input file open error.\n");
    return INVALID_INPUT;
  }
  if (!(output_fp = fopen(argv[2], "w"))){
    printf("output file open error.\n");
    return INVALID_INPUT;
  }
  if (!(result_fp = fopen(argv[3], "w"))){
    printf("result file open error.\n");
    return INVALID_INPUT;
  }
  dataword_size = (size_t)atoi(argv[5]);
  if ((dataword_size != 4) && (dataword_size != 8)){
    printf("dataword size must be 4 or 8.\n");
    return INVALID_INPUT;
  }

  /* parse generator to binary (argv[4])*/
  parse_string_to_binary(argv[4], generator, &generator_size);
  codeword_size = dataword_size + generator_size - 1;

  return VALID_INPUT;
}

void shift_left(unsigned char *binary_arr, int num, int max_size) {
  for (int i = 0 ; i < num ; i++){
    binary_arr[0] = SHIFT_LEFT(binary_arr[0],1);
    for(int j=1; j < max_size ; j++) { // shift overflow 처리
        if ((binary_arr[j] & msb) == msb) {
          binary_arr[j-1] |= lsb;
        }
        binary_arr[j] = SHIFT_LEFT(binary_arr[j],1);
      }
  }
}

void shift_right(unsigned char *binary_arr, int num, int max_size) {
  for (int i = 0 ; i<num ; i++){
    binary_arr[max_size - 1] = SHIFT_RIGHT(binary_arr[max_size - 1],1);
    for(int j=max_size - 2; j >= 0 ; j--) { // shift overflow 처리
        if ((binary_arr[j] & lsb) == lsb) {
          binary_arr[j+1] |= msb;
        }
        binary_arr[j] = SHIFT_RIGHT(binary_arr[j],1);
      }
  }
}

void decode(){
  int i;
  unsigned char tmp[MAX_BYTES] = {0x00,};
  for (i = 0 ; i < MAX_BYTES; i++) {
    tmp[i] |= mem_codewords[i];
  }

  /* divide */
  int shift_num = (int)(dataword_size);
  while (shift_num > 0) {
    if ((tmp[0] & msb) == msb){
      for(int i = 0; i < MAX_BYTES ; i++) {
          tmp[i] = tmp[i]^generator[i]; // XOR 연산
      }
    }
    shift_left(tmp, 1, MAX_BYTES);
    shift_num--;
  }

  /* validate remainder */
  for (i=0 ; i < generator_size - 1 ; i++) {
    if ((tmp[0] & msb) == msb) {
        error_num++;
        break;
    }
    shift_left(tmp, 1, MAX_BYTES);
  }


  return;
}

void get_file_size(FILE *fp) {
  fseek(input_fp, 0, SEEK_END);
  input_file_size = ftell(input_fp);
  fseek(input_fp, 0, 0);
  return;
}

int main(int argc, char **argv){
    unsigned char ch;
    unsigned char saved_dataword;
    int bytes_to_read;

    /* Initialize and validate input arguments */
    if (init(argc, argv) == INVALID_INPUT){
      return 0;
    }

    /* memory allocation on codeword buffer */
    get_file_size(input_fp);
    
    mem_codewords = (unsigned char*)malloc(input_file_size);
    fread(mem_codewords, sizeof(unsigned char), input_file_size, input_fp);

    /* read padding bits number */
    unsigned char padding_bits_num_binary = mem_codewords[0];
    //int padding_bits_num = binaryToInt(padding_bits_num_binary);
    int padding_bits_num = (input_file_size - 1)*8 % codeword_size;

    shift_left(mem_codewords, 8, input_file_size);
    shift_left(mem_codewords, padding_bits_num, input_file_size);

    int total_codeword_num = ((input_file_size - 1)*8 - padding_bits_num) / codeword_size;

    /* decode */
    int i;
    int eof = 0;
    while(1) {
      saved_dataword = 0x00;
      if (dataword_size == 4) {
        /* upper codeword */
        saved_dataword |= (UPPER_MASK & mem_codewords[0]);
        decode();
        codeword_num++;

        shift_left(mem_codewords, codeword_size, input_file_size);
        /* lower codeword */
        saved_dataword |= SHIFT_RIGHT((UPPER_MASK & mem_codewords[0]), 4);
        decode();
        codeword_num++;

      } else { // dataword_size == 8
        saved_dataword |= mem_codewords[0];
        //decode
        decode();
        codeword_num++;
      }

      /* write to decoded file */
      fwrite(&saved_dataword, sizeof(char), 1, output_fp);

      if (total_codeword_num <= codeword_num){
        break;
      }
      shift_left(mem_codewords, codeword_size, input_file_size);
    }

    /* write to result file*/
    fprintf(result_fp, "%d %d", codeword_num, error_num);

    /* close file descriptors */
    fclose(input_fp);
    fclose(output_fp);
    fclose(result_fp);
    return 0;
}
/*
//compile
gcc -o crc_decoder_20181610 crc_decoder_20181610.cc
gcc -o crc_encoder_20181610 crc_encoder_20181610.cc

//cmd
./crc_encoder_20181610 datastream.tx codedstream.tx 1101 4
./crc_decoder_20181610 codedstream.tx datastream.rx result.txt 1101 4

./crc_encoder_20181610 test.tx codedstream.tx 1101 4



//test
./crc_encoder_20181610 datastream.tx codedstream.tx 1101 4
./linksim codedstream.tx codedstream.rx 0.05 1001
./crc_decoder_20181610 codedstream.rx datastream.rx result.txt 1101 4


//test
./crc_encoder_20181610 datastream.tx codedstream.tx 10100010111010001011101000101110100010111010001011101000101110100010111010001011101000101110100010111010001011101000101110100010111010001011101000101110100010111010001011101000101110100010111010001011 4
./linksim codedstream.tx codedstream.rx 0.0 1001
./crc_decoder_20181610 codedstream.rx datastream.rx result.txt 10100010111010001011101000101110100010111010001011101000101110100010111010001011101000101110100010111010001011101000101110100010111010001011101000101110100010111010001011101000101110100010111010001011 4

1011001101101100110110110011011011001101101100110110110011011011001101101100110110110011011011001101
*/