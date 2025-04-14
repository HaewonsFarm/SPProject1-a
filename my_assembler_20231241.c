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
static int assem_pass2(void);
void make_opcode_output(char* file_name);
void make_symtab_output(char* file_name);
void make_literaltab_output(char* filename);
void make_objectcode_output(char* file_name);

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
    if (init_my_assembler() < 0)
    {
        printf("init_my_assembler: 프로그램 초기화에 실패 했습니다.\n");
        return -1;
    }

    if (assem_pass1() < 0)
    {
        printf("assem_pass1: 패스1 과정에서 실패하였습니다.  \n");
        return -1;
    }


    make_symtab_output("output_symtab.txt");
    make_literaltab_output("output_littab.txt");
    if (assem_pass2() < 0)
    {
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
    
    // 주석 라인 (첫 글자 '.')
    if (str[0] == '.') {
        strncpy(t->comment, str, sizeof(t->comment) - 1);
        t->label = strdup("");
        t->operator = strdup("");
        t->operand[0] = strdup("");
        token_table[token_line++] = t;
        return 0;
    }
    
    str = trim(str);
    if (strlen(str) == 0) {
        // 빈 줄이면 토큰 구조를 추가하지 않고 그냥 끝냄
        free(t);
        return 0;
    }
    char *saveptr;
    char *token_str = strtok_r(str, " \t", &saveptr);
    int count = 0;
    while (token_str != NULL && count < MAX_COLUMNS) {
        token_str = trim(token_str);
        if (count == 0) {
            // 첫 토큰: 조건에 따라 operator로 할당하거나 label로 할당
            if (strcasecmp(token_str, "END") == 0 ||
                (search_opcode(token_str) >= 0) ||
                (token_str[0] == '+' && search_opcode(token_str + 1) >= 0)) {
                t->operator = strdup(token_str);
                t->label = strdup("");
            } else {
                t->label = strdup(token_str);
                // operator가 설정되지 않으면 기본적으로 빈 문자열로 초기화
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
    if (t->operator && strlen(t->operator) > 0) {
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
    /* input_data의 문자열을 한줄씩 입력 받아서
     * token_parsing()을 호출하여 _token에 저장
     */
    /* 모든 소스 라인에 대해 토큰 파싱 */
    for (int i = 0; i < line_num; i++) {
        char* line_copy = strdup(input_data[i]);
        if (token_parsing(line_copy) < 0) {
            free(line_copy);
            return -1;
        }
        free(line_copy);
    }
    
    locctr = 0;
    for (int i = 0; i < token_line; i++) {
        token* t = token_table[i];
        if (t->comment[0] == '.')
            continue;
        
        if (strcasecmp(t->operator, "START") == 0) {    // t->operator가 NULL인 경우 strcasecmp에 전달되어 EXC_BAD_ACCESS 오류(널 포인터 접근 오류) 발생
            locctr = (int)strtol(t->operand[0], NULL, 16);
            if (strlen(t->label) > 0) {
                strcpy(sym_table[label_num].symbol,t->label);
                sym_table[label_num].addr = locctr;
                label_num++;
            }
            continue;
        }
        
        if (strlen(t->label) > 0) {
            strcpy(sym_table[label_num].symbol, t->label);
            sym_table[label_num].addr = locctr;
            label_num++;
        }
        
        /* 리터럴 처리: operand가 '='로 시작하면 literal_table 에 저장 */
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
        
        if (strcasecmp(t->operator, "WORD") == 0) {
            locctr += 3;
        } else if (strcasecmp(t->operator, "RESW") == 0) {
            int num = atoi(t->operand[0]);
            locctr += 3 * num;
        } else if (strcasecmp(t->operator, "RESB") == 0) {
            int num = atoi(t->operand[0]);
            locctr += num;
        } else if (strcasecmp(t->operator, "BYTE") == 0) {
            if (t->operand[0][0] == 'C' || t->operand[0][0]=='c') {
                char* start = strchr(t->operand[0], '\'');
                char* end = strrchr(t->operand[0], '\'');
                if (start && end && end > start)
                    locctr += (end - start - 1);
            } else if (t->operand[0][0] == 'X' || t->operand[0][0]=='x') {
                char* start = strchr(t->operand[0], '\'');
                char* end = strrchr(t->operand[0], '\'');
                if (start && end && end > start)
                    locctr += ((end - start - 1) + 1) / 2;
            }
        } else if (strcasecmp(t->operator, "LTORG") == 0) {
            // 등록된 리터럴에 대해 주소 할당
            for (int j = 0; j < literal_count; j++) {
                if (literal_table[j].addr == -1) {
                    char* lit = literal_table[j].symbol;
                    int length = 0;
                    if (lit[1] == 'C' || lit[1] == 'c') {
                        char* start = strchr(lit, '\'');
                        char* end = strrchr(lit, '\'');
                        if (start && end && end > start)
                            length = end - start - 1;
                    } else if (lit[1] == 'X' || lit[1] == 'x') {
                        char* start = strchr(lit, '\'');
                        char* end = strrchr(lit, '\'');
                        if (start && end && end > start)
                            length = ((end - start - 1) + 1) / 2;
                    }
                    literal_table[j].addr = locctr;
                    locctr += length;
                }
            }
        } else if (strcasecmp(t->operator, "END") == 0) {
            for (int j = 0; j < literal_count; j++) {
                if (literal_table[j].addr == -1) {
                    char* lit = literal_table[j].symbol;
                    int length = 0;
                    if (lit[1] == 'C' || lit[1] == 'c') {
                        char* start = strchr(lit, '\'');
                        char* end = strrchr(lit, '\'');
                        if (start && end && end > start)
                            length  = end - start - 1;
                    } else if (lit[1] == 'X' || lit[1] == 'x') {
                        char* start = strchr(lit, '\'');
                        char* end = strrchr(lit, '\'');
                        if (start && end && end > start)
                            length = ((end - start - 1) + 1) / 2;
                    }
                    literal_table[j].addr = locctr;
                    locctr += length;
                }
            }
            break;
        } else {
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
        fprintf(fp, "%-8s %04X\n", sym_table[i].symbol, sym_table[i].addr);
    }
    if (fp != stdout)
        fclose(fp);
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
        fprintf(fp, "%-8s %04X\n", literal_table[i].symbol, literal_table[i].addr);
    }
    if (fp != stdout)
        fclose(fp);
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
    /* assem_pass2 함수: 본 예제에서는 추가 처리 없이 0 리턴(추후 object code 생성 확장)*/
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
    FILE* fp;
    if (file_name == NULL)
        fp = stdout;
    else {
        fp = fopen(file_name, "w");
        if (!fp) {
            perror("Error opening object code output file");
            return;
        }
    }
    fprintf(fp, "Object code generation not implemented.\n");
    if (fp!= stdout)
        fclose(fp);
}
