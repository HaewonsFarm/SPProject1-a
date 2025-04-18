/*
 * 파일명 : my_assembler_20231241.c
 * 설  명 : 이 프로그램은 SIC/XE 머신을 위한 간단한 Assembler 프로그램의 메인루틴으로,
 * 입력된 파일의 코드 중, 명령어에 해당하는 OPCODE를 찾아 출력한다.
 * 파일 내에서 사용되는 문자열 "00000000"에는 자신의 학번을 기입한다.
 */

/*
 *
 * 프로그램의 헤더를 정의한다.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <strings.h>

// 파일명의 "00000000"은 자신의 학번으로 변경할 것.
#include "my_assembler_20231241.h"

// 토큰 파싱 시 라벨, operator, operand 총 3개
#define MAX_COLUMNS 3
#define MAX_TEXT_RECORD_LENGTH 30   // Text record 최대 바이트 수
#define MAX_EXTREF 100

/* 전역 변수 정의 */
inst* inst_table[MAX_INST];
int inst_index = 0;

char* input_data[MAX_LINES];
int line_num = 0;

int label_num = 0;

token* token_table[MAX_LINES];
int token_line = 0;

symbol sym_table[MAX_LINES];
symbol literal_table[MAX_LINES];

int locctr = 0;

char* input_file;
char* output_file;

int literal_count = 0;  // 리터럴 테이블 항목 수
int literalPoolStart = 0;   // 현재 섹션의 미처리 리터럴 시작 인덱스

int current_section = 1;    // 현재 섹션 번호 관리

char extref_table[MAX_EXTREF][32];
int extref_count = 0;

int total_program_end = 0;  // 전제 길이 저장용 전역 변수

/* 함수 선언부 */
int init_my_assembler(void);
int init_inst_file(char* inst_file);
int init_input_file(char* input_file_name);
char* trim(char* str);
void to_upper(char* s);
int token_parsing(char* str);
int search_opcode(char* str);
int get_instruction_length(char* op);
static int assem_pass1(void);
void make_symtab_output(char* file_name);
void make_literaltab_output(char* filename);
void extract_literal(const char* literalStr, char* dest);
void process_literal_pool(void);
static int get_register_number(const char *r);
int calc_disp(int target, int current, int format, int base, int e, int *b, int *p);
void calc_nixbpe(token* t, int baseOpcode, int *finalOpcode, int *n, int *i, int *x, int *e, int *targetAddr);
char* generate_object_code(token* t);
char* generate_modification_record(token* t);
static int assem_pass2(void);
void make_opcode_output(char* file_name);
void make_objectcode_output(char* file_name);
int is_extref(const char* symbol);

/* ----------------------------------------------------------------------------------
 * 설명 : 사용자로 부터 어셈블리 파일을 받아서 명령어의 OPCODE를 찾아 출력한다.
 * 매개 : 실행 파일, 어셈블리 파일
 * 반환 : 성공 = 0, 실패 = < 0
 * 주의 : 현재 어셈블리 프로그램의 리스트 파일을 생성하는 루틴은 만들지 않았다.
 *           또한 중간파일을 생성하지 않는다.
 * ----------------------------------------------------------------------------------
 */
int main(int args, char *arg[])
{
    if (init_my_assembler() < 0) {
        printf("init_my_assembler: 프로그램 초기화에 실패 했습니다.\n");
        return -1;
    }

    if (assem_pass1() < 0) {
        printf("assem_pass1: 패스1 과정에서 실패하였습니다.  \n");
        return -1;
    }


    make_symtab_output("output_symtab.txt");
    make_literaltab_output("output_littab.txt");
    
    if (assem_pass2() < 0) {
        printf(" assem_pass2: 패스2 과정에서 실패하였습니다.  \n");
        return -1;
    }

    make_opcode_output("opcode_output.txt");
    make_objectcode_output("output_objectcode.txt");
    
    return 0;
}

/* ----------------------------------------------------------------------------------
 * 설명 : 프로그램 초기화를 위한 자료구조 생성 및 파일을 읽는 함수이다.
 * 매개 : 없음
 * 반환 : 정상종료 = 0 , 에러 발생 = -1
 * 주의 : 각각의 명령어 테이블을 내부에 선언하지 않고 관리를 용이하게 하기
 *           위해서 파일 단위로 관리하여 프로그램 초기화를 통해 정보를 읽어 올 수 있도록
 *           구현하였다.
 * ----------------------------------------------------------------------------------
 */
int init_my_assembler(void)
{
    int result;

    if ((result = init_inst_file("inst_table.txt")) < 0)
        return -1;
    if ((result = init_input_file("input-1.txt")) < 0)
        return -1;
    return result;
}

/* ----------------------------------------------------------------------------------
 * 설명 : 머신을 위한 기계 코드목록 파일(inst_table.txt)을 읽어
 *       기계어 목록 테이블(inst_table)을 생성하는 함수이다.
 *
 *
 * 매개 : 기계어 목록 파일
 * 반환 : 정상종료 = 0 , 에러 < 0
 * 주의 : 기계어 목록파일 형식은 자유롭게 구현한다. 예시는 다음과 같다.
 *
 *    ===============================================================================
 *           | 이름 | 형식 | 기계어 코드 | 오퍼랜드의 갯수 | \n |
 *    ===============================================================================
 *
 * ----------------------------------------------------------------------------------
 */
int init_inst_file(char *inst_file)
{
    FILE* fp = fopen(inst_file, "r");
    
    if (!fp) {
        perror("Error opening instruction file");
        return -1;
    }
    
    char line[100];
    inst_index = 0;
    while(fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] == '\n')
            continue;
        inst* new_inst = (inst*)malloc(sizeof(inst));
        if (!new_inst) {
            fclose(fp);
            return -1;
        }
        int format, ops;
        char op_hex[3];
        if (sscanf(line, "%s %d %2s %d", new_inst->str, &format, op_hex, &ops) < 4) {
            free(new_inst);
            continue;
        }
        new_inst->format = format;
        new_inst->ops = ops;
        new_inst->op = (unsigned char)strtol(op_hex, NULL, 16);
        inst_table[inst_index++] = new_inst;
    }
    fclose(fp);
    return 0;
}

/* ----------------------------------------------------------------------------------
 * 설명 : 어셈블리 할 소스코드를 읽어 소스코드 테이블(input_data)를 생성하는 함수이다.
 * 매개 : 어셈블리할 소스파일명
 * 반환 : 정상종료 = 0 , 에러 < 0
 * 주의 : 라인단위로 저장한다.
 *
 * ----------------------------------------------------------------------------------
 */
int init_input_file(char *input_file_name)
{
    FILE* fp = fopen(input_file_name, "r");
    if (!fp) {
        perror("Error opening input file");
        return -1;
    }
    char line[256];
    line_num = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        input_data[line_num] = strdup(line);
        line_num++;
        if (line_num >= MAX_LINES)
            break;
    }
    fclose(fp);
    return 0;
}

// trim 함수: 문자열 앞뒤 공백 제거
char* trim(char* str) {
    char* end;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return str;
}

// to_upper 함수: 문자열을 모두 대문자로 변환
void to_upper(char* s) {
    for (int i = 0; s[i] != '\0'; i++) {
        s[i] = toupper((unsigned char)s[i]);
    }
}

/* ----------------------------------------------------------------------------------
 * 설명 : 소스 코드를 읽어와 토큰단위로 분석하고 토큰 테이블을 작성하는 함수이다.
 *        패스 1로 부터 호출된다.
 * 매개 : 파싱을 원하는 문자열
 * 반환 : 정상종료 = 0 , 에러 < 0
 * 주의 : my_assembler 프로그램에서는 라인단위로 토큰 및 오브젝트 관리를 하고 있다.
 * ----------------------------------------------------------------------------------
 */
/* token_parsing 함수: 한 줄의 어셈블리 소스를 free-format 방식으로 파싱
 - 첫 토큰이 "END" 혹은 opcode라면 label 없이 operator에 저장
 - 그 외의 경우 첫 토큰은 label, 두 번째는 operator, 세 번째는 operand */
int token_parsing(char *str)
{
    if (str == NULL)
        return -1;
    
    token* t = (token*)malloc(sizeof(token));
    if (!t)
        return -1;
    memset(t, 0, sizeof(token));
    
    // 앞뒤 공백 제거
    str = trim(str);
    
    // 만약 전체 문자열이 빈 문자열이면 바로 종료
    if (strlen(str) == 0) {
        free(t);
        return 0;
    }
    
    // 주석 라인 처리 (첫 글자 '.')
    if (str[0] == '.') {
        strncpy(t->comment, str, sizeof(t->comment) - 1);
        t->label = strdup("");
        t->operator = strdup("");
        t->operand[0] = strdup("");
        token_table[token_line++] = t;
        return 0;
    }
    
    char *saveptr;
    char *token_str = strtok_r(str, " \t", &saveptr);
    int count = 0;
    while (token_str != NULL && count < MAX_COLUMNS) {
        token_str = trim(token_str);  // 각 토큰별 공백 제거
        if (count == 0) {
            // 첫 토큰: 만약 토큰이 "END", "LTORG", "EXTDEF", "EXTREF" 등 지시어와 정확히 일치하면
            // 이를 operator에 저장합니다.
            if (strcasecmp(token_str, "END") == 0 ||
                strcasecmp(token_str, "LTORG") == 0 ||
                strcasecmp(token_str, "EXTDEF") == 0 ||
                strcasecmp(token_str, "EXTREF") == 0 ||
                (search_opcode(token_str) >= 0) ||
                (token_str[0] == '+' && search_opcode(token_str + 1) >= 0)) {
                t->operator = strdup(token_str);
                t->label = strdup("");
            } else {
                t->label = strdup(token_str);
                t->operator = strdup("");
            }
        } else if (count == 1) {
            if (t->operator == NULL || strlen(t->operator) == 0)
                t->operator = strdup(token_str);
            else
                t->operand[0] = strdup(token_str);
        } else if (count == 2) {
            if (t->operand[0] == NULL || strlen(t->operand[0]) == 0)
                t->operand[0] = strdup(token_str);
        }
        count++;
        token_str = strtok_r(NULL, " \t", &saveptr);
    }
    
    if (t->operator == NULL)
        t->operator = strdup("");
    if (t->operand[0] == NULL)
        t->operand[0] = strdup("");
    if (strlen(t->operator) > 0) {
        to_upper(t->operator);
    }
    
    token_table[token_line++] = t;
    return 0;
}

/* ----------------------------------------------------------------------------------
 * 설명 : 입력 문자열이 기계어 코드인지를 검사하는 함수이다.
 * 매개 : 토큰 단위로 구분된 문자열
 * 반환 : 정상종료 = 기계어 테이블 인덱스, 에러 < 0
 * 주의 : 기계어 목록 테이블에서 특정 기계어를 검색하여, 해당 기계어가 위치한 인덱스를 반환한다.
 *        '+JSUB'과 같은 문자열에 대한 처리는 자유롭게 처리한다.
 *
 * ----------------------------------------------------------------------------------
 */
int search_opcode(char *str)
{
    if (str == NULL)
        return -1;
    char temp[20];
    strncpy(temp, str, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';
    if (temp[0] == '+') {
        memmove(temp, temp+1, strlen(temp));
    }
    to_upper(temp);
    for (int i = 0; i < inst_index; i++) {
        if (strcasecmp(inst_table[i]->str, temp) == 0) {
            return inst_table[i]->op;
        }
    }
    return -1;
}

/* get_instruction_length 함수: 해당 operator에 따른 명령어 길이(형식)을 리턴
 - '+'가 선행되면 format 4, 그 외는 inst_tabledml format 필드 참조 */
int get_instruction_length(char* op) {
    if (op == NULL)
        return 0;
    if (op[0] == '+') {
        char temp[20];
        strncpy(temp, op+1, sizeof(temp)-1);
        temp[sizeof(temp)-1] = '\0';
        to_upper(temp);
        for (int i = 0; i < inst_index; i++) {
            if (strcasecmp(inst_table[i]-> str, temp) == 0) {
                return 4;
            }
        }
        return 4;   // 기본적으로 format 4
    } else {
        char temp[20];
        strncpy(temp, op, sizeof(temp)-1);
        temp[sizeof(temp)-1] = '\0';
        to_upper(temp);
        for (int i = 0; i < inst_index; i++) {
            if (strcasecmp(inst_table[i]->str, temp) == 0) {
                return inst_table[i]->format;
            }
        }
    }
    return 0;
}

/* ----------------------------------------------------------------------------------
* 설명 : 어셈블리 코드를 위한 패스1과정을 수행하는 함수이다.
*           패스1에서는..
*           1. 프로그램 소스를 스캔하여 해당하는 토큰단위로 분리하여 프로그램 라인별 토큰
*           테이블을 생성한다.
*          2. 토큰 테이블은 token_parsing()을 호출하여 설정한다.
*          3. assem_pass2 과정에서 사용하기 위한 심볼테이블 및 리터럴 테이블을 생성한다.
*
* 매개 : 없음
* 반환 : 정상 종료 = 0 , 에러 = < 0
* 주의 : 현재 초기 버전에서는 에러에 대한 검사를 하지 않고 넘어간 상태이다.
*      따라서 에러에 대한 검사 루틴을 추가해야 한다.
*
* -----------------------------------------------------------------------------------
*/
static int assem_pass1(void)
{
    // 모든 소스 라인에 대해 토큰 파싱
    for (int i = 0; i < line_num; i++) {
        char* line_copy = strdup(input_data[i]);
        if (token_parsing(line_copy) < 0) {
            free(line_copy);
            return -1;
        }
        free(line_copy);
    }
    
    // 첫 번째 라인의 경우, 만약 label이 비어있고, operator가 "START"가 아니면
    // operator를 프로그램 이름(label)으로 취급
    if (token_line > 0 &&
        strlen(token_table[0]->label) == 0 &&
        strlen(token_table[0]->operator) > 0 &&
        strcasecmp(token_table[0]->operator, "START") != 0) {
        token_table[0]->label = strdup(token_table[0]->operator);
        token_table[0]->operator[0] = '\0';
    }
    
    locctr = 0;
    literalPoolStart = 0;
    current_section = 1;  // 첫 섹션
    
    for (int i = 0; i < token_line; i++) {
        token* t = token_table[i];
        if (t->comment[0] == '.')
            continue;
        if (t->operator == NULL)
            t->operator = strdup("");
        
        // START 지시어: locctr 초기화
        if (strcasecmp(t->operator, "START") == 0) {
            locctr = (int)strtol(t->operand[0], NULL, 16);
            if (strlen(t->label) > 0) {
                strcpy(sym_table[label_num].symbol, t->label);
                sym_table[label_num].addr = locctr;
                sym_table[label_num].section = current_section;
                label_num++;
            }
            continue;
        }
        
        // CSECT 지시어: 섹션 전환 → pending literal 처리 후,
        // locctr를 0으로 리셋하고 current_section 증가
        if (strcasecmp(t->operator, "CSECT") == 0) {
            process_literal_pool();
            locctr = 0;
            literalPoolStart = literal_count;
            current_section++;
            if (strlen(t->label) > 0) {
                strcpy(sym_table[label_num].symbol, t->label);
                sym_table[label_num].addr = locctr;
                sym_table[label_num].section = current_section;
                label_num++;
            }
            continue;
        }
        
        // EQU 지시어 처리 (예: MAXLEN EQU BUFEND-BUFFER)
        if (strcasecmp(t->operator, "EQU") == 0) {
            int value = 0;
            if (strcmp(t->operand[0], "*") == 0) {
                value = locctr;
            }
            else if (strchr(t->operand[0], '-') != NULL) {
                char left[32] = {0}, right[32] = {0};
                sscanf(t->operand[0], "%[^-]-%s", left, right);
                int leftVal = -1, rightVal = -1;
                for (int k = 0; k < label_num; k++) {
                    if (strcmp(sym_table[k].symbol, left) == 0)
                        leftVal = sym_table[k].addr;
                    if (strcmp(sym_table[k].symbol, right) == 0)
                        rightVal = sym_table[k].addr;
                }
                if (leftVal != -1 && rightVal != -1)
                    value = leftVal - rightVal;
            }
            else {
                value = (int)strtol(t->operand[0], NULL, 16);
            }
            if (strlen(t->label) > 0) {
                strcpy(sym_table[label_num].symbol, t->label);
                sym_table[label_num].addr = value;
                sym_table[label_num].section = current_section;
                label_num++;
            }
            continue;
        }
        
        // EXTDEF, EXTREF는 심볼 테이블에 등록하지 않음
        if (strcasecmp(t->operator, "EXTDEF") == 0 ||
            strcasecmp(t->operator, "EXTREF") == 0 ||
            strcasecmp(t->label, "EXTDEF") == 0 ||
            strcasecmp(t->label, "EXTREF") == 0) {
            continue;
        }
        
        // 일반 심볼: 라벨이 있으면 등록
        if (strlen(t->label) > 0) {
            strcpy(sym_table[label_num].symbol, t->label);
            sym_table[label_num].addr = locctr;
            sym_table[label_num].section = current_section;
            label_num++;
        }
        
        // 리터럴 처리: operand가 '='로 시작하면 등록 (pending literal)
        if (t->operand[0] && t->operand[0][0] == '=') {
            int found = 0;
            for (int j = 0; j < literal_count; j++) {
                if (strcmp(literal_table[j].symbol, t->operand[0]) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                strcpy(literal_table[literal_count].symbol, t->operand[0]);
                literal_table[literal_count].addr = -1;
                literal_count++;
            }
        }
        
        // directive/명령어에 따라 locctr 증분
        if (strcasecmp(t->operator, "WORD") == 0) {
            locctr += 3;
        } else if (strcasecmp(t->operator, "RESW") == 0) {
            int num = atoi(t->operand[0]);
            locctr += 3 * num;
        } else if (strcasecmp(t->operator, "RESB") == 0) {
            int num = atoi(t->operand[0]);
            locctr += num;
        } else if (strcasecmp(t->operator, "BYTE") == 0) {
            if (t->operand[0][0]=='C' || t->operand[0][0]=='c') {
                char* start = strchr(t->operand[0], '\'');
                char* end = strrchr(t->operand[0], '\'');
                if (start && end && end > start)
                    locctr += (end - start - 1);
            } else if (t->operand[0][0]=='X' || t->operand[0][0]=='x') {
                char* start = strchr(t->operand[0], '\'');
                char* end = strrchr(t->operand[0], '\'');
                if (start && end && end > start)
                    locctr += ((end - start - 1) + 1) / 2;
            }
        }
        else if (strcasecmp(t->operator, "LTORG") == 0) {
            process_literal_pool();
        }
        else if (strcasecmp(t->operator, "END") == 0) {
            process_literal_pool();
            break;
        }
        else {
            int ins_length = get_instruction_length(t->operator);
            locctr += ins_length;
        }
    }
    return 0;
}

/* ----------------------------------------------------------------------------------
* 설명 : 입력된 문자열의 이름을 가진 파일에 프로그램의 결과를 저장하는 함수이다.
*
* 매개 : 생성할 오브젝트 파일명
* 반환 : 없음
* 주의 : 소스코드 명령어 앞에 OPCODE가 기록된 코드를 파일에 출력한다.
*        파일이 NULL값이 들어온다면 프로그램의 결과를 stdout으로 보내어
*        화면에 출력해준다.
*        프로젝트 1에서는 불필요하다.
 *
* -----------------------------------------------------------------------------------
*/
void make_opcode_output(char *file_name)
{
    FILE* fp;
    if (file_name == NULL)
        fp = stdout;
    else {
        fp = fopen(file_name, "w");
        if (!fp) {
            perror("Error opening output file");
            return;
        }
    }
    
    for (int i = 0; i < token_line; i++) {
        token* t = token_table[i];
        if (t->comment[0] == '.') {
            fprintf(fp, "%s\n", t->comment);
            continue;
        }
        if (t->label && strlen(t->label) > 0)
            fprintf(fp, "%-8s", t->label);
        else
            fprintf(fp, "\t");
        if (t->operator && strlen(t->operator) > 0)
            fprintf(fp, "%-8s", t->operator);
        else fprintf(fp, "\t");
        if (t->operand[0] && strlen(t->operand[0]) > 0)
            fprintf(fp, "%-16s", t->operand[0]);
        else
            fprintf(fp, "\t");
        int opcode = search_opcode(t->operator);
        if (opcode >= 0)
            fprintf(fp, "\t%02X", opcode);
        fprintf(fp, "\n");
    }
    if (fp != stdout)
        fclose(fp);
}

/* ----------------------------------------------------------------------------------
* 설명 : 입력된 문자열의 이름을 가진 파일에 프로그램의 결과를 저장하는 함수이다.
*        여기서 출력되는 내용은 SYMBOL별 주소값이 저장된 TABLE이다.
* 매개 : 생성할 오브젝트 파일명 혹은 경로
* 반환 : 없음
* 주의 : 파일이 NULL값이 들어온다면 프로그램의 결과를 stdout으로 보내어
*        화면에 출력해준다.
*
* -----------------------------------------------------------------------------------
*/
void make_symtab_output(char *file_name)
{
    FILE* fp;
    if (file_name == NULL)
        fp = stdout;
    else {
        fp = fopen(file_name, "w");
        if (!fp) {
            perror("Error opening symtab output file");
            return;
        }
    }
    
    for (int i = 0; i < label_num; i++) {
        if (i > 0 && sym_table[i].section != sym_table[i-1].section)
            fprintf(fp, "\n"); // 섹션 변경 시 개행
        fprintf(fp, "%-8s\t%X\n", sym_table[i].symbol, sym_table[i].addr);
    }
    
    if (fp != stdout)
        fclose(fp);
}

// literal의 내부 내용을 추출하는 함수
void extract_literal(const char* literalStr, char* dest) {
    if (literalStr == NULL || dest == NULL)
        return;
    // literalStr이 '='로 시작하고, 그 다음에 'C' 혹은 'X'가 있는 경우
    if(literalStr[0]=='=' && (literalStr[1]=='C' || literalStr[1]=='c' ||
                              literalStr[1]=='X' || literalStr[1]=='x')) {
        const char* start = strchr(literalStr, '\'');
        const char* end = strrchr(literalStr, '\'');
        if(start != NULL && end != NULL && end > start) {
            size_t len = end - start - 1;
            strncpy(dest, start + 1, len);
            dest[len] = '\0';
            return;
        }
    }
    // 그 외는 그대로 복사
    strcpy(dest, literalStr);
}

/* 현재 섹션의 미할당 리터럴에 대해 현재 locctr 값을 할당하고
   literal의 길이만큼 locctr를 증가시키며, literalPoolStart를 갱신 */
void process_literal_pool(void) {
    for (int j = literalPoolStart; j < literal_count; j++) {
        if (literal_table[j].addr == -1) {
            literal_table[j].addr = locctr;
            char* lit = literal_table[j].symbol;
            int length = 0;
            if (lit[1]=='C' || lit[1]=='c') {
                char* start = strchr(lit, '\'');
                char* end = strrchr(lit, '\'');
                if (start && end && end > start)
                    length = end - start - 1;
            } else if (lit[1]=='X' || lit[1]=='x') {
                char* start = strchr(lit, '\'');
                char* end = strrchr(lit, '\'');
                if (start && end && end > start)
                    length = ((end - start - 1) + 1) / 2;
            }
            locctr += length;
        }
    }
    literalPoolStart = literal_count;
}

/* ----------------------------------------------------------------------------------
* 설명 : 입력된 문자열의 이름을 가진 파일에 프로그램의 결과를 저장하는 함수이다.
*        여기서 출력되는 내용은 LITERAL별 주소값이 저장된 TABLE이다.
* 매개 : 생성할 오브젝트 파일명
* 반환 : 없음
* 주의 : 파일이 NULL값이 들어온다면 프로그램의 결과를 stdout으로 보내어
*        화면에 출력해준다.
*
* -----------------------------------------------------------------------------------
*/
// 리터럴 테이블을 16진수 주소와 함께 출력
void make_literaltab_output(char* file_name)
{
    FILE* fp;
    if (file_name == NULL)
        fp = stdout;
    else {
        fp = fopen(file_name, "w");
        if (!fp) {
            perror("Error opening literaltab output file");
            return;
        }
    }
    
    for (int i = 0; i < literal_count; i++) {
        char litValue[32] = {0};
        extract_literal(literal_table[i].symbol, litValue);
        fprintf(fp, "%-8s\t%X\n", litValue, literal_table[i].addr);
    }
    if (fp != stdout)
        fclose(fp);
}

// get_register_number(): 레지스터 번호 매핑
static int get_register_number(const char *r) {
    if (strcasecmp(r, "A") == 0) return 0;
    else if (strcasecmp(r, "X") == 0) return 1;
    else if (strcasecmp(r, "L") == 0) return 2;
    else if (strcasecmp(r, "B") == 0) return 3;
    else if (strcasecmp(r, "S") == 0) return 4;
    else if (strcasecmp(r, "T") == 0) return 5;
    else if (strcasecmp(r, "F") == 0) return 6;
    
    return 0;
}

// PC-Relative, Base-Relative disp 계산
int calc_disp(int target, int current, int format, int base, int e, int *b, int *p) {
    int instrLen = (format == 4 || e == 1) ? 4 : 3;
    int pc = current + instrLen;
    int disp = target - pc;

    // PC-relative
    if (disp >= -2048 && disp <= 2047) {
        *b = 0;
        *p = 1;
        return disp & 0xFFF;  // 12비트 보정
    }

    // BASE-relative fallback
    disp = target - base;
    if (disp >= 0 && disp <= 4095) {
        *b = 1;
        *p = 0;
        return disp & 0xFFF;
    }

    // format 4가 아닌데 범위를 벗어나면 오류
    if (format == 3 && !e) {
        fprintf(stderr, "Error: displacement out of range at address %X\n", current);
        *b = 0;
        *p = 0;
        return 0;
    }

    // format 4일 경우 disp는 20비트 → generate_modification_record()에서 수정
    *b = 0;
    *p = 0;
    return 0;
}

/* ------------------- 모듈화된 op와 nixbpe 계산 함수 ------------------- */
/* calc_nixbpe()
   - t             : 현재 토큰 (token 구조체 포인터)
   - baseOpcode    : OPCODE 테이블에서 검색한 기본 opcode (8비트)
   - finalOpcode   : 최종 opcode (n, i 비트 적용 후)을 리턴 (포인터)
   - n, i, x, e    : 각각 n, i, indexed(x), extended(e) 비트를 리턴 (포인터)
   - targetAddr    : operand를 통해 결정된 목표 주소를 리턴 (포인터)
   
   주소 지정 방식:
     • 만약 operand가 '#'로 시작하면 즉시 addressing (n=0,i=1)
     • 만약 operand가 '@'로 시작하면 간접 addressing (n=1,i=0)
     • 그 외에는 직접 addressing (n=1,i=1); operand에 ",X"가 포함된 경우 x=1 처리
     • operator 앞에 '+'가 있으면 format 4로 e=1
*/
void calc_nixbpe(token* t, int baseOpcode, int *finalOpcode, int *n, int *i, int *x, int *e, int *targetAddr) {
    *n = 0; *i = 0; *x = 0; *e = 0;
    
    // extended format 확인: operator에 '+'가 있으면 format 4
    if (t->operator[0] == '+')
        *e = 1;
    
    // 주소 지정 방식 및 targetAddr 결정:
    if (t->operand[0] && t->operand[0][0] == '#') {
        // Immediate: operand가 숫자 상수일 것으로 가정
        *n = 0; *i = 1;
        *targetAddr = (int)strtol(t->operand[0] + 1, NULL, 16);
    } else if (t->operand[0] && t->operand[0][0] == '@') {
        // Indirect addressing: '@' 제거하고 심볼 테이블 조회
        *n = 1; *i = 0;
        char operandCopy[64];
        strcpy(operandCopy, t->operand[0] + 1);
        int found = 0;
        for (int j = 0; j < label_num; j++) {
            if (strcmp(sym_table[j].symbol, operandCopy) == 0) {
                *targetAddr = sym_table[j].addr;
                found = 1;
                break;
            }
        }
        if (!found)
            *targetAddr = 0;  // 심볼 미발견 시 0 처리 (필요 시 에러 처리)
    } else {
        // Direct addressing: 기본값 n=1, i=1
        *n = 1; *i = 1;
        char operandCopy[64];
        strcpy(operandCopy, t->operand[0]);
        // ",X" 확인하여 indexed addressing 여부 처리
        char* comma = strstr(operandCopy, ",X");
        if (comma != NULL) {
            *x = 1;
            *comma = '\0';  // ",X" 삭제하여 심볼 이름만 남김
        }
        int found = 0;
        for (int j = 0; j < label_num; j++) {
            if (strcmp(sym_table[j].symbol, operandCopy) == 0) {
                *targetAddr = sym_table[j].addr;
                found = 1;
                break;
            }
        }
        if (!found) {
            // 심볼이 없으면 숫자로 처리 (예: 상수값)
            *targetAddr = (int)strtol(operandCopy, NULL, 16);
        }
    }
    
    // 최종 opcode: 원래 baseOpcode의 상위 6비트에 n, i 비트를 덮어씌움
    *finalOpcode = (baseOpcode & 0xFC) | ((*n << 1) | *i);
}

/* generate_object_code(): 주어진 토큰에 대해 object code 문자열 생성 (format 3/4)
   이 함수에서는 opcode의 하위 2비트를 ni 비트로 채우고
   PC-relative 방식으로 disp를 계산하는 로직을 구현함.
 */
char* generate_object_code(token* t) {
    int format = get_instruction_length(t->operator);
    
    int baseOpcode = search_opcode(t->operator);
    if (baseOpcode < 0) baseOpcode = 0;
    
    // format 2 처리
    if (format == 2) {
        char opnd1[16] = {0}, opnd2[16] = {0};
        sscanf(t->operand[0], "%15[^,],%15s", opnd1, opnd2);
        int r1 = get_register_number(opnd1);
        int r2 = get_register_number(opnd2);
        unsigned int instr = (baseOpcode << 8) | (r1 << 4) | r2;
        char *obj = malloc(5);
        sprintf(obj, "%04X", instr);
        return obj;
    }
    
    // format 3/4 처리
    int finalOpcode, n, i, x, e;
    int targetAddr;
    calc_nixbpe(t, baseOpcode, &finalOpcode, &n, &i, &x, &e, &targetAddr);
    
    // 여기서 t->addr는 해당 명령어의 시작 주소를 저장한다고 가정 (pass1에서 설정)
    int currentAddr = t->addr;
    
    /* BASE relative 용 BASE 레지스터 값
       BASE 지시어 처리 시 global 변수 (예: base_register)를 설정해두면 좋습니다.
       여기서는 기본값 0으로 처리합니다.
    */
    int base = 0 ;   // 전역 base_register 값 또는 0
    
    int flag_b = 0, flag_p = 0;
    // format 4이면 e == 1 → disp는 0으로 채워두고 M rec에서 수정
    int disp = (e ? 0 :
                calc_disp(targetAddr, currentAddr, format, base, e, &flag_b, &flag_p));
    
    // 4비트의 flag: x (bit3), b (bit2), p (bit1), e (bit0)
    int flags = (x << 3) | (flag_b << 2) | (flag_p << 1) | e;
    
    unsigned int instr;
    char* objStr;
    
    if (format == 3) {
        // Format 3: 24비트 → 8비트 opcode, 4비트 flags, 12비트 disp
        instr = (finalOpcode << 16)| (flags << 12) | (disp & 0xFFF);
        objStr = malloc(7);  // 6 hex digits + null terminator
        sprintf(objStr, "%06X", instr);
    }
    else {  // Format 4
        instr = (finalOpcode << 24) | (flags << 20) | (disp & 0xFFFFF);
        objStr = (char*)malloc(9);  // 8 hex digits + null terminator
        sprintf(objStr, "%08X", instr);
    }
    return objStr;
}

// format 4인 경우 M 레코드
char* generate_modification_record(token* t) {
    if (!t || !t->operand[0]) return NULL;

    // format 4: M 레코드 주소는 instr addr + 1
    int mod_addr = t->addr + 1;
    char* op = t->operand[0];
    char* mod = malloc(32);
    if (!mod) return NULL;

    // BUFEND - BUFFER 처리
    if (strchr(op, '-')) {
        char sym1[32], sym2[32];
        sscanf(op, "%31[^-]-%31s", sym1, sym2);
        if (is_extref(sym1))
            sprintf(mod, "M%06X06+%s", mod_addr, sym1);
        else if (is_extref(sym2))
            sprintf(mod, "M%06X06-%s", mod_addr, sym2);
        else {
            free(mod);
            return NULL;
        }
    }
    // 단일 EXTREF symbol 처리
    else if (is_extref(op)) {
        sprintf(mod, "M%06X05+%s", mod_addr, op);
    } else {
        free(mod);
        return NULL;
    }

    return mod;
}

/* ----------------------------------------------------------------------------------
* 설명 : 어셈블리 코드를 기계어 코드로 바꾸기 위한 패스2 과정을 수행하는 함수이다.
*           패스 2에서는 프로그램을 기계어로 바꾸는 작업은 라인 단위로 수행된다.
*           다음과 같은 작업이 수행되어 진다.
*           1. 실제로 해당 어셈블리 명령어를 기계어로 바꾸는 작업을 수행한다.
* 매개 : 없음
* 반환 : 정상종료 = 0, 에러발생 = < 0
* 주의 :
* -----------------------------------------------------------------------------------
*/
static int assem_pass2(void)
{
    // H, T, M, E 레코드 생성
    // token_table, sym_table, literal_table을 바탕으로 각 섹션별로 Object Code를 생성하여 파일에 출력
    FILE *fp;
    fp = fopen("output_objectcode.txt", "w");
    if (!fp) {
        perror("Error opening object code output file");
        return -1;
    }
    
    // 각 control section 별로 object code를 생성함.
    // token_table의 순서대로 섹션이 연속된다고 가정하고 처리
    int i = 0;
    while(i < token_line) {
        // H 레코드 생성
        token *first = token_table[i];
        int secStart = first->addr;
        char progName[7] = {0};
        if (strlen(first->label) > 0)
            strncpy(progName, first->label, 6);
        else
            strncpy(progName, first->operator, 6);

        // 정확한 object code의 끝 위치 추적
        int j = i;
        int last_objcode_end = secStart;
        
        int last_code_addr = -1;
        int last_code_len = -1;

        while (j < token_line) {
            token *t = token_table[j];

            // 섹션 종료 조건
            if (j > i && (!strcasecmp(t->operator, "CSECT") || !strcasecmp(t->operator, "END")))
                break;

            int len = get_instruction_length(t->operator);
            if (t->operator[0] == '+' && len == 0) len = 4;

            // Object code를 생성하지 않는 directive 제외
            if (!(!t->operator[0] ||
                !strcasecmp(t->operator, "START") ||
                !strcasecmp(t->operator, "CSECT") ||
                !strcasecmp(t->operator, "END") ||
                !strcasecmp(t->operator, "EQU") ||
                !strcasecmp(t->operator, "EXTDEF") ||
                !strcasecmp(t->operator, "EXTREF") ||
                !strcasecmp(t->operator, "LTORG"))) {
                if (t->addr + len > last_objcode_end)
                    last_objcode_end = t->addr + len;
            }

            j++;
        }

        int secLength = (last_code_addr + last_code_len) - secStart;

        // 전체 프로그램 길이 갱신 (전역 변수 필요: int total_program_end = 0;)
        if ((last_code_addr + last_code_len) > total_program_end)
            total_program_end = last_code_addr + last_code_len;

        // 첫 섹션만 전체 길이 포함
        if (i == 0)
            fprintf(fp, "H %-6s %06X %06X\n", progName, secStart, total_program_end - secStart);
        else
            fprintf(fp, "H %-6s %06X %06X\n", progName, secStart, secLength);
        
        // D, R 레코드 생성
        char dRecord[256] = {0};
        char rRecord[256] = {0};
        for (int k = i; k < j; k++) {
            token *t = token_table[k];
            if (strcasecmp(t->operator, "EXTDEF") == 0) {
                char *def = strtok(t->operand[0], ",");
                while(def != NULL) {
                    // EXTDEF의 주소는 심볼 테이블에서 찾아야 함 (여기서는 0으로 처리한 예)
                    char tmp[16];
                    sprintf(tmp, "%-6s %06X ", def, 0);
                    strcat(dRecord, tmp);
                    def = strtok(NULL, ",");
                }
            }
            if (strcasecmp(t->operator, "EXTREF") == 0) {
                char *ref = strtok(t->operand[0], ",");
                while(ref != NULL) {
                    strcpy(extref_table[extref_count++], ref);
                    strcat(rRecord, ref);
                    strcat(rRecord, " ");
                    ref = strtok(NULL, ",");
                }
            }
        }
        if (strlen(dRecord) > 0)
            fprintf(fp, "D %s\n", dRecord);
        if (strlen(rRecord) > 0)
            fprintf(fp, "R %s\n", rRecord);
        
        // T, M 레코드 생성
        int tRecStart = -1;
        char tRecord[MAX_TEXT_RECORD_LENGTH * 2 + 1] = {0};
        int tRecLength = 0;
        char modRecords[100][32];
        int modCount = 0;

        // T, M 레코드 생성 루프 내부
        for (int k = i; k < j; k++) {
            token *t = token_table[k];
            
            if (k > i && (!strcasecmp(t->operator, "CSECT") || !strcasecmp(t->operator, "END")))
                break;

            if (!t->operator[0] ||
                !strcasecmp(t->operator, "START") ||
                !strcasecmp(t->operator, "CSECT") ||
                !strcasecmp(t->operator, "END") ||
                !strcasecmp(t->operator, "EQU") ||
                !strcasecmp(t->operator, "EXTDEF") ||
                !strcasecmp(t->operator, "EXTREF") ||
                !strcasecmp(t->operator, "LTORG"))
                continue;

            // 실제 object code 생성
            char *obj = generate_object_code(t);
            if (obj && *obj) {
                int objBytes = strlen(obj) / 2;

                // ✅ H 레코드용 마지막 위치 추적
                last_code_addr = t->addr;
                last_code_len = objBytes;

                if (tRecLength == 0) tRecStart = t->addr;
                if (tRecLength + objBytes > MAX_TEXT_RECORD_LENGTH) {
                    fprintf(fp, "T %06X %02X %s\n", tRecStart, tRecLength, tRecord);
                    tRecord[0] = '\0';
                    tRecLength = 0;
                    tRecStart = t->addr;
                }

                strcat(tRecord, obj);
                tRecLength += objBytes;
            }

            // M 레코드 생성
            if (t->operator[0] == '+') {
                char* mod = generate_modification_record(t);
                if (mod) {
                    strcpy(modRecords[modCount++], mod);
                    free(mod);
                }
            }

            free(obj);  // ✅ 여기서 해줘야 안전
        }
        
        // 남은 T 레코드 flush
        if (tRecLength > 0)
            fprintf(fp, "T %06X %02X %s\n", tRecStart, tRecLength, tRecord);
        // 출력한 모든 M 레코드
        for (int m = 0; m < modCount; m++)
            fprintf(fp, "%s\n", modRecords[m]);
        
        // E 레코드: 섹션 시작 주소 기록
        fprintf(fp, "E %06X\n", secStart);
        
        // 다음 섹션으로 건너뛰기
        // i를 섹션 끝까지 올림 (END/CSECT 인덱스까지)
        while (i < token_line &&
               strcasecmp(token_table[i]->operator, "CSECT") != 0 &&
               strcasecmp(token_table[i]->operator, "END") != 0) {
            i++;
        }
        
        if ((last_code_addr + last_code_len) > total_program_end)
            total_program_end = last_code_addr + last_code_len;
        
        // 첫 CSECT/END 토큰도 넘어가도록
        i++;
    }
    
    fclose(fp);
    return 0;
}

/* ----------------------------------------------------------------------------------
* 설명 : 입력된 문자열의 이름을 가진 파일에 프로그램의 결과를 저장하는 함수이다.
*        여기서 출력되는 내용은 object code이다.
* 매개 : 생성할 오브젝트 파일명
* 반환 : 없음
* 주의 : 파일이 NULL값이 들어온다면 프로그램의 결과를 stdout으로 보내어
*        화면에 출력해준다.
*        명세서의 주어진 출력 결과와 완전히 동일해야 한다.
*        예외적으로 각 라인 뒤쪽의 공백 문자 혹은 개행 문자의 차이는 허용한다.
*
* -----------------------------------------------------------------------------------
*/
void make_objectcode_output(char *file_name)
{
    FILE* fp = fopen(file_name, "r");
    if (fp == NULL) {
        perror("Error reading object code output file");
        return;
    }
    char ch;
    while ((ch = fgetc(fp)) != EOF)
        putchar(ch);
    fclose(fp);
}

// is_extref(): EXTREF 여부 확인 함수
int is_extref(const char* symbol) {
    for (int i = 0; i < extref_count; i++) {
        if (strcmp(extref_table[i], symbol) == 0)
            return 1;
    }
    return 0;
}
