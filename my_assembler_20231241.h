#ifndef MY_ASSEMBLER_20231241_H
#define MY_ASSEMBLER_20231241_H

/*
 * my_assembler 함수를 위한 변수 선언 및 매크로를 담고 있는 헤더 파일이다.
 *
 */
#define MAX_INST 256
#define MAX_LINES 5000
#define MAX_OPERAND 3

 /*
  * instruction 목록 파일로 부터 정보를 받아와서 생성하는 구조체 변수이다.
  * 라인 별로 하나의 instruction을 저장한다.
  */
typedef struct _inst
{
    char str[10];
    unsigned char op;
    int format;
    int ops;
} inst;

extern inst* inst_table[MAX_INST];
extern int inst_index;

/*
 * 어셈블리 할 소스코드를 입력받는 테이블이다. 라인 단위로 관리할 수 있다.
 */
extern char* input_data[MAX_LINES];
extern int line_num;

/*
 * 어셈블리 할 소스코드를 토큰단위로 관리하기 위한 구조체 변수이다.
 * operator는 renaming을 허용한다.
 */
typedef struct _token
{
    char* label;
    char* operator;
    char* operand[MAX_OPERAND];
    char comment[100];
    char nixbpe;
    int addr;   // 명령어의 주소 정보를 저장하기 위해 추가하였다.
    int section;    // 명령어의 섹션 정보를 저장하기 위해 추가하였다.
} token;

extern token* token_table[MAX_LINES];
extern int token_line;

/*
 * 심볼을 관리하는 구조체이다.
 * 심볼 테이블은 심볼 이름, 심볼의 위치로 구성된다.
 * 추후 과제에 사용 예정
 */
typedef struct _symbol
{
    char symbol[10];
    int addr;
    int section;    // 심볼이 속한 섹션 번호를 저장하기 위해 추가하였다.
} symbol;

/*
* 리터럴을 관리하는 구조체이다.
* 리터럴 테이블은 리터럴의 이름, 리터럴의 위치로 구성된다.
* 추후 과제에 사용 예정
*/
typedef struct _literal {
    char literal[20];
    int addr;
} literal;

extern symbol sym_table[MAX_LINES];
extern symbol literal_table[MAX_LINES];


/**
 * 오브젝트 코드 전체에 대한 정보를 담는 구조체이다.
 * Header Record, Define Recode,
 * Modification Record 등에 대한 정보를 모두 포함하고 있어야 한다. 이
 * 구조체 변수 하나만으로 object code를 충분히 작성할 수 있도록 구조체를 직접
 * 정의해야 한다.
 */

// Object Code 전체 정보를 담기 위하 구조체
typedef struct _object_code {
    /* add fields */
} object_code;


extern int locctr;
//--------------

extern char* input_file;
extern char* output_file;

/* 함수 프로토타입 */
int init_my_assembler(void);
int init_inst_file(char* inst_file);
int init_input_file(char* input_file);
char* trim(char* str);
void to_upper(char* s);
int token_parsing(char* str);
int search_opcode(char* str);
int get_instruction_length(char* op);
static int assem_pass1(void);
static int assem_pass2(void);
void make_opcode_output(char* file_name);
void make_symtab_output(char* file_name);
void make_literaltab_output(char* file_name);
void make_objectcode_output(char* file_name);

#endif
