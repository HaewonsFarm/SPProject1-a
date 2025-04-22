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
#define MAX_SECTIONS 10

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
int locctr_table[MAX_LINES];
char* input_file;
char* output_file;
int literal_count = 0;  // 리터럴 테이블 항목 수
int literalPoolStart = 0;   // 현재 섹션의 미처리 리터럴 시작 인덱스
int current_section = 1;    // 현재 섹션 번호 관리
int section_length[MAX_SECTIONS];
char extref_table[MAX_EXTREF][32];
int extref_count = 0;
int total_program_end = 0;  // 전제 길이 저장용 전역 변수
int base = 0;
int literalPoolStartSec[MAX_SECTIONS+1];    // 섹션마다 리터럴 시작 인덱스 저장
int literalPoolEndSec[MAX_SECTIONS+1];
int sectionStartAddr[MAX_SECTIONS+1];

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
void calc_nixbpe(token* t, int baseOpcode, int *finalOpcode, int *n, int *i,int *x, int *e, int *targetAddr);
char* generate_object_code(token* t);
char** generate_modification_records(token* t, int* count);
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
    if (str == NULL) return -1;

    // 1) 앞뒤 공백 제거
    str = trim(str);

    // 2) 빈 라인 → 무시
    if (strlen(str) == 0) return 0;

    // 3) 주석 라인
    if (str[0] == '.') {
        token* t = calloc(1, sizeof(token));
        if (!t) return -1;
        strncpy(t->comment, str, sizeof(t->comment)-1);
        t->label    = strdup("");
        t->operator = strdup("");
        t->operand[0] = strdup("");
        token_table[token_line++] = t;
        return 0;
    }

    // 4) 일반 명령어/지시어 라인
    token* t = calloc(1, sizeof(token));
    if (!t) return -1;
    t->label      = strdup("");
    t->operator   = strdup("");
    t->operand[0] = strdup("");

    char *saveptr, *tok;
    int count = 0;

    tok = strtok_r(str, " \t", &saveptr);
    while (tok && count < MAX_COLUMNS) {
        tok = trim(tok);

        if (count == 0) {
            // 첫 토큰: 지시어/명령어라면 operator, 아니면 label
            if (!strcasecmp(tok, "END")  ||
                !strcasecmp(tok, "LTORG")||
                !strcasecmp(tok, "EXTDEF")||
                !strcasecmp(tok, "EXTREF")||
                search_opcode(tok) >= 0 ||
                (tok[0]=='+' && search_opcode(tok+1) >= 0))
            {
                free(t->operator);
                t->operator = strdup(tok);
            } else {
                free(t->label);
                t->label = strdup(tok);
            }
        }
        else if (count == 1) {
            if (strlen(t->operator) == 0) {
                // operator 아직 비어 있으면 이 토큰을 operator로
                free(t->operator);
                t->operator = strdup(tok);
            } else {
                // operator 이미 있으면 이 토큰이 operand
                free(t->operand[0]);
                t->operand[0] = strdup(tok);
                break;  // 남은 건 모두 무시
            }
        }
        else {
            // count==2: 세 번째 토큰도 operand로 덮어쓰기
            free(t->operand[0]);
            t->operand[0] = strdup(tok);
            break;
        }

        count++;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    // 5) operator 대문자 정리
    if (t->operator) to_upper(t->operator);

    // 6) operator 없으면 에러
    if (!t->operator || strlen(t->operator)==0) {
        free(t->label);
        free(t->operator);
        free(t->operand[0]);
        free(t);
        return -1;
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
    // 1) 토큰 파싱 및 테이블 구축
    for (int i = 0; i < line_num; i++) {
        char* line_copy = strdup(input_data[i]);
        if (token_parsing(line_copy) < 0) {
            free(line_copy);
            return -1;
        }
        free(line_copy);
    }

    // 2) 초기값 설정
    locctr = 0;
    literalPoolStart = 0;
    current_section = 1;
    literalPoolStartSec[current_section] = 0;   // 섹션 1은 0부터
    literalPoolEndSec[current_section] = 0;
    sectionStartAddr[current_section] = locctr;

    // 3) 패스1 주요 루프: 각 토큰별로 주소 기록 및 locctr 증가
    for (int i = 0; i < token_line; i++) {
        token* t = token_table[i];
        // 3.1) 현재 locctr을 토큰의 주소로 저장
        t->addr = locctr;
        locctr_table[i] = locctr;

        // 3.2) 주석 라인
        if (t->comment[0] == '.')
            continue;

        /// 3.3) START 지시어
        if (!strcasecmp(t->operator, "START")) {
            // 프로그램 시작 주소로 locctr 설정
            locctr = (int)strtol(t->operand[0], NULL, 16);
            sectionStartAddr[current_section] = locctr;

            // ▶ START 다음에 label(COPY)이 있으면 symtab에 추가
            if (strlen(t->label) > 0) {
                strcpy(sym_table[label_num].symbol, t->label);
                sym_table[label_num].addr    = locctr;
                sym_table[label_num].section = current_section;
                label_num++;
            }
            continue;
        }

        // 3.4) CSECT 지시어: 섹션 전환 및 리터럴 풀 처리
        if (!strcasecmp(t->operator, "CSECT")) {
            process_literal_pool();

            // 이전 섹션의 리터럴 풀 종료 인덱스 기록
            section_length[current_section] = locctr;
            literalPoolEndSec[current_section] = literal_count;

            if (locctr > total_program_end)
                total_program_end = locctr;

            locctr = 0;
            literalPoolStart = literal_count;
            current_section++;
            literalPoolStartSec[current_section] = literal_count;
            sectionStartAddr[current_section] = 0;  // csect는 항상 0으로 리셋

            // ▶ CSECT 다음에 label(RDREC, WRREC)이 있으면 symtab에 추가
            if (strlen(t->label) > 0) {
                strcpy(sym_table[label_num].symbol, t->label);
                sym_table[label_num].addr    = locctr;
                sym_table[label_num].section = current_section;
                label_num++;
            }

            continue;

            // 여기서 locctr은 계산이 잘 된다.
        }

        // LTORG 또는 END 시점에 리터럴 풀 처리
        if (!strcasecmp(t->operator, "LTORG")) {
            process_literal_pool();
            literalPoolEndSec[current_section] = literal_count;
            continue;
        }
        if (!strcasecmp(t->operator, "END")) {
            process_literal_pool();
            literalPoolEndSec[current_section] = literal_count;
            section_length[current_section] = locctr;
            break;
        }

        // 3.5) EQU, EXTDEF, EXTREF 등 기타 지시어 처리 및 심볼 테이블 등록
        if (!strcasecmp(t->operator, "EQU")) {
            int value = 0;
            // 1) '*' 이면 현재 주소
            if (strcmp(t->operand[0], "*") == 0) {
                value = t->addr;
            }
            // 2) 'SYM1-SYM2' 형태이면 두 심볼의 차이
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
            // 3) 그 외는 상수(16진수)로 파싱
            else {
                value = (int)strtol(t->operand[0], NULL, 16);
            }

            if (strlen(t->label) > 0) {
                strcpy(sym_table[label_num].symbol, t->label);
                sym_table[label_num].addr    = value;
                sym_table[label_num].section = current_section;
                label_num++;
            }
            continue;
        }        if (!strcasecmp(t->operator, "EXTDEF") || !strcasecmp(t->operator, "EXTREF"))
            continue;

        // 3.6) 라벨이 있으면 심볼 테이블에 추가 (같은 섹션 내에서만 중복 체크)
        if (strlen(t->label) > 0) {
            int exists = 0;
            for (int s = 0; s < label_num; s++) {
                if (!strcmp(sym_table[s].symbol, t->label) 
                    && sym_table[s].section == current_section) {
                    exists = 1;
                    break;
                }
            }
            if (!exists) {
                strcpy(sym_table[label_num].symbol, t->label);
                sym_table[label_num].addr    = t->addr;
                sym_table[label_num].section = current_section;
                label_num++;
            }
        }

        // 3.7) 리터럴 수집: operand가 '='로 시작하면 리터럴 테이블에 등록 (TD/WD는 수집 안함)
        if (t->operand[0] && t->operand[0][0] == '=') {
            int found = 0;
            for (int j = 0; j < literal_count; j++)
                if (!strcmp(literal_table[j].symbol, t->operand[0])) { found = 1; break; }
            if (!found) {
                strcpy(literal_table[literal_count].symbol, t->operand[0]);
                literal_table[literal_count].addr = -1;
                literal_count++;
            }
        }

        // 3.x) BASE, NOBASE 지시어 처리
        if (!strcasecmp(t->operator, "BASE")) {
            // sym_table에서 t->operand[0] 심볼의 addr 찾아서 base에 저장
            for (int k = 0; k < label_num; k++) {
                if (!strcmp(sym_table[k].symbol, t->operand[0])) {
                    base = sym_table[k].addr;
                    break;
                }
            }
            continue;
        }
        if (!strcasecmp(t->operator, "NOBASE")) {
            base = 0;
            continue;
        }

        // 3.8) 지시어/명령어 길이만큼 locctr 증가
        if (!strcasecmp(t->operator, "WORD"))              locctr += 3;
        else if (!strcasecmp(t->operator, "RESW")) { int n=atoi(t->operand[0]); locctr += 3*n; }
        else if (!strcasecmp(t->operator, "RESB")) { int n=atoi(t->operand[0]); locctr += n; }
        else if (!strcasecmp(t->operator, "BYTE")) {
            char *start = strchr(t->operand[0], '\'');
            char *end   = strrchr(t->operand[0], '\'');
            if (start && end && end>start) {
                if (toupper((unsigned char)t->operand[0][0]) == 'C')
                    locctr += end - start - 1;
                else  // X
                    locctr += (end - start - 1 + 1) / 2;
            }
        }
        else if (!strcasecmp(t->operator, "LTORG"))        process_literal_pool();
        else if (!strcasecmp(t->operator, "END")) { 
            process_literal_pool(); 
            section_length[current_section] = locctr;
            break;
        }
        else {                                            // 형식 1~4 명령어
            locctr += get_instruction_length(t->operator);
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
    // 항상 b/p 비트를 0으로 초기화
    *b = 0;
    *p = 0;

    // format 4 (extended) 명령어는 disp 필드를 0으로 채우고 M-record로 처리
    if (e == 1 || format == 4) {
        return 0;
    }

    // instruction 길이(PC 계산용)
    int instrLen = 3;  // format 3
    int pc = current + instrLen;
    int disp = target - pc;

    // PC-relative: -2048 ≤ disp ≤ +2047
    if (disp >= -2048 && disp <= 2047) {
        *p = 1;
        if (disp < 0) {
            disp = 0x1000 + disp;
        }
        return disp & 0xFFF;        // 하위 12비트로 자르기
    }

    // Base-relative: 0 ≤ (target - base) ≤ 4095
    disp = target - base;
    if (disp >= 0 && disp <= 4095) {
        *b = 1;
        return disp & 0xFFF;
    }

    // 둘 다 실패하면, format 4로 업그레이드
    if (disp < -2048 || disp > 4095) {
        e = 1;    // e는 로컬 변수라 caller에서는 무효함.
        *b = *p = 0;
        return 0;
    }
    return 0;  // format 4 의 disp 필드는 0
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
void calc_nixbpe(token* t, int baseOpcode,
                 int *finalOpcode, int *n, int *i,
                 int *x, int *e, int *targetAddr)
{
    // 1) nixbpe 플래그 0으로 초기화
    *n = *i = *x = *e = 0;

    // literal
    if (t->operand[0] && t->operand[0][0] == '=') {
        *n = 1; *i = 1;
        // literal address 찾기
        for (int j = 0; j < literal_count; j++) {
            if (strcmp(literal_table[j].symbol, t->operand[0]) == 0) {
                *targetAddr = literal_table[j].addr;
                break;
            }
        }
        *finalOpcode = (baseOpcode & 0xFC) | 0x03;
        return;
    }

    // 2) extended format인지 확인
    if (t->operator[0] == '+') {
        *e = 1;
    }

    // 3) immediate addressing
    if (t->operand[0] && t->operand[0][0] == '#') {
        *n = 0; *i = 1;
        // 상수 literal이면 (#5, #0x10, etc.), 각각 parsing
        if (isdigit((unsigned char)t->operand[0][1])) {
            *n = 0;
            *targetAddr = (int)strtol(t->operand[0] + 1, NULL, 16);
        }
        else {
            // symbolic immediate (#LABEL 주소 검색)
            *n = 0;
            char symcpy[64];
            strcpy(symcpy, t->operand[0] + 1);
            for (int j = 0; j < label_num; j++) {
                if (strcmp(sym_table[j].symbol, symcpy) == 0) {
                    *targetAddr = sym_table[j].addr;
                    break;
                }
            }
        }
    }
    // 4) indirect addressing
    else if (t->operand[0] && t->operand[0][0] == '@') {
        *n = 1;  *i = 0;
        char symcpy[64];
        strcpy(symcpy, t->operand[0] + 1);
        for (int j = 0; j < label_num; j++) {
            if (strcmp(sym_table[j].symbol, symcpy) == 0) {
                *targetAddr = sym_table[j].addr;
                break;
            }
        }
    }
    // 5) symple(direct) addressing
    else {
        *n = 1;  *i = 1;
        char symcpy[64];
        strcpy(symcpy, t->operand[0]);
        // ",X" 가 나오면 x=1로 바꾸고 parsing
        char *comma = strstr(symcpy, ",X");
        if (comma) {
            *x = 1;
            *comma = '\0';
        }
        // symbol 찾기
        int found = 0;
        for (int j = 0; j < label_num; j++) {
            if (strcmp(sym_table[j].symbol, symcpy) == 0) {
                *targetAddr = sym_table[j].addr;
                found = 1;
                break;
            }
        }
        // 상수로 변환
        if (!found) {
            *targetAddr = (int)strtol(symcpy, NULL, 16);
        }
    }

    // 6) ni 비트를 최종 오피코드로 만들기
    *finalOpcode = (baseOpcode & 0xFC) | ((*n << 1) | *i);
}

/* 토큰 포인터->인덱스 변환 (locctr_table 사용을 위해) */
static int findTokenIndex(token *t) {
    for (int idx = 0; idx < token_line; idx++) {
        if (token_table[idx] == t) return idx;
    }
    return -1;
}

// 토큰이 T 레코드에 들어갈 만한 instruction 혹은 BYTE/WORD/리터럴인가?
int isTextRecordable(token *t) {
    if (t->comment[0] == '.')       return 0; // 주석
    if (t->operator[0] == '\0')     return 0; // 빈 라벨
    // 이하 object code 없는 지시어
    if (!strcasecmp(t->operator,"START") ||
        !strcasecmp(t->operator,"END")   ||
        !strcasecmp(t->operator,"CSECT") ||
        !strcasecmp(t->operator,"EXTDEF")||
        !strcasecmp(t->operator,"EXTREF")||
        !strcasecmp(t->operator,"EQU")   ||
        !strcasecmp(t->operator,"RESW")  ||
        !strcasecmp(t->operator,"RESB")  ||
        !strcasecmp(t->operator,"LTORG")) 
        return 0;
    return 1;
}

/* generate_object_code(): locctr_table 기반으로 disp 계산 */
char* generate_object_code(token* t) {
    printf("GEN_OBJ: op=\"%s\", fmt=%d, operand=\"%s\"\n",
           t->operator,
           get_instruction_length(t->operator),
           t->operand[0] ? t->operand[0] : "");

    // 0) 미리 opcode 뽑아두기
    int baseOpcode = search_opcode(t->operator);
    if (baseOpcode < 0) baseOpcode = 0;

    // RSUB 처리 (n=i=1)
    if (strcasecmp(t->operator, "RSUB") == 0) {
        unsigned int opcode = search_opcode("RSUB");      // 0x4F
        unsigned int finalOpc = (opcode & 0xFC) | 0x03;   // n=1,i=1 → 0x4F|0x03 = 0x4F
        unsigned int instr = finalOpc << 16;              // format3 → 3바이트
        char *obj = malloc(7);
        sprintf(obj, "%06X", instr);
        return obj;
    }

    // 0) I/O format‑3 명령어 처리 (TD, WD) – 리터럴은 위에서 처리, 그 외는 아래 else 로 넘어감
    if (!strcasecmp(t->operator, "TD") || !strcasecmp(t->operator, "WD")) {
        const char *lit = t->operand[0];
        if (lit[0] == '=') lit++;               // “=X'05'” 처럼 = 로 시작하면 건너뛰고
        if (lit[0] == 'X' && lit[1] == '\'') {
            // 리터럴 X'..' 형태
            int deviceCode = strtol(lit + 2, NULL, 16);
            unsigned int opcode = (search_opcode(t->operator) & 0xFC) | 0x03; // n=1,i=1
            unsigned int instr = opcode << 16        // opcode+ni
                                | (0          << 12) // x=b=p=e 모두 0
                                | (deviceCode & 0xFFF);
            char *obj = malloc(7);
            sprintf(obj, "%06X", instr);
            return obj;
        }
        else {
            // ---- 여기가 핵심: 심볼(예: TD INPUT) 주소 지정 처리 ----
            int finalOpc, n, i, x, e, targetAddr;
            calc_nixbpe(t, baseOpcode, &finalOpc, &n, &i, &x, &e, &targetAddr);

            int idx         = findTokenIndex(t);
            int currentAddr = locctr_table[idx];
            int flag_b = 0, flag_p = 0;

            // base 레지스터 사용 시 설정
            int disp = calc_disp(targetAddr, currentAddr, /*format=*/3, base, e, &flag_b, &flag_p);

            int flags = (x << 3) | (flag_b << 2) | (flag_p << 1) | e;
            unsigned int instr = (finalOpc << 16) | (flags << 12) | (disp & 0xFFF);
            char *obj = malloc(7);
            sprintf(obj, "%06X", instr);
            return obj;
        }
    }

    // 1) BYTE 지시어 처리 (C'...' 또는 X'...')
    if (!strcasecmp(t->operator, "BYTE")) {
        const char *opnd = t->operand[0];
        if (opnd[0]=='C' && opnd[1]=='\'') {
            int len = strlen(opnd) - 3;    // C'..' → 실제 문자 개수
            char *obj = malloc(len*2 + 1);
            obj[0] = '\0';
            for (int i = 2; i < 2+len; i++) {
                char hex[3];
                sprintf(hex, "%02X", (unsigned char)opnd[i]);
                strcat(obj, hex);
            }
            return obj;
        } else if (opnd[0]=='X' && opnd[1]=='\'') {
            int len = strlen(opnd) - 3;    // X'..' → hex 길이
            char *obj = malloc(len + 1);
            memcpy(obj, opnd + 2, len);
            obj[len] = '\0';
            return obj;
        }
    }

    // 2) WORD 지시어 처리 (상수, 심볼 또는 “심볼1-심볼2” 표현식)
    if (!strcasecmp(t->operator, "WORD")) {
        char *operand = t->operand[0];
        // 일단 0으로 채움
        if (strchr(operand, '-') || isalpha((unsigned char)operand[0])) {
            char *obj = malloc(7);
            strcpy(obj, "000000");
            return obj;
        }
        // 순수 상수 (e.g., WORD 5)이면 기존처럼 처리
        int value = (int)strtol(operand, NULL, 16);
        char* obj = malloc(7);
        sprintf(obj, "%06X", value);
        return obj;
        }

    if (t->operand[0]) {
        trim(t->operand[0]);
        // 쉼표만 남았을 때도 제거
        size_t L = strlen(t->operand[0]);
        if (L > 0 && t->operand[0][L-1] == ',')
            t->operand[0][L-1] = '\0';
    }

    int format = get_instruction_length(t->operator);   // 명령어 format 추출

    // # 숫자 분기: LDA #3 같은 경우
    // 여기서 바로 opcode, n, i, flags, disp 값을 계산 후 리턴
    if (format != 2 &&
        t->operand[0][0] == '#' &&
        isdigit((unsigned char)t->operand[0][1])) {
        printf("DEBUG immediate: op=\"%s\", baseOpcode=0x%02X\n",
               t->operand[0], baseOpcode);



        // 숫자 파싱: '#3' -> 3
         int value = (int)strtol(t->operand[0] + 1, NULL, 0);
         printf("DEBUG parsed value = %d (0x%X)\n", value, value);

        // n = 0, i = 1, x=b=p=e=0
        unsigned int opcode = (baseOpcode & 0xFC) | 0x01;
        unsigned int flags = 0;

        // format 3: 6자리 16진수 (3 바이트)
        unsigned int instr = (opcode << 16) | (flags << 12) | (value & 0xFFF);
        char *obj = malloc(7);
        sprintf(obj, "%06X", instr);
        return obj;
    }

    // Format 2: 레지스터 형식
    if (format == 2) {
        char opnd1[16] = {0}, opnd2[16] = {0};
        int count = sscanf(t->operand[0], "%15[^,],%15s", opnd1, opnd2);

        int r1 = get_register_number(opnd1);
        int r2;
        if (count == 2) {
            // , 뒤에 2번째 레지스터가 있을 때만 반환
            r2 = get_register_number(opnd2);
        } else {
            // 없으면 0
            r2 = 0;
        }
        unsigned int instr = (baseOpcode << 8) | (r1 << 4) | r2;
        char *obj = malloc(5);
        if (!obj) {
            perror("malloc failed");
            return NULL;
        }
        sprintf(obj, "%04X", instr);
        return obj;
    }

    // Format 3/4 계산을 위해 각 플래그 및 OP 계산
    int finalOpcode, n, i, x, e, targetAddr;
    calc_nixbpe(t, baseOpcode, &finalOpcode, &n, &i, &x, &e, &targetAddr);  // opcode 리턴

    // 현재 명령어의 주소를 locctr_table에서 얻음
    int idx = findTokenIndex(t);
    int currentAddr = locctr_table[idx];
    
    // disp 계산 전 플래그 초기화
    int flag_b = 0, flag_p = 0;
    int disp;

    // 간접 주소(@)가 숫자 상수일 경우: 16진수로 파싱
    if (t->operand[0][0] == '@' && isdigit((unsigned char)t->operand[0][1])) {
        disp = (int)strtol(t->operand[0] + 1, NULL, 0);
        flag_b = 0; flag_p = 0;
    } else {
        // format 4인 경우엔 disp=0, M 레코드로 처리
        if (e) {
            disp = 0;
        } else {
            disp = calc_disp(targetAddr, currentAddr, format, base, e, &flag_b, &flag_p);
        }
    }

    // nixbpe 플래그를 6비트 플래그로 인코딩 (x 비트는 이미 calc_nixbpe에서 설정됨)
    int flags = (x << 3) | (flag_b << 2) | (flag_p << 1) | e;

    // opcode 구성
    unsigned int instr;
    char *objStr;
    if (format == 3) {
        // 6자리 16진수
        instr = (finalOpcode << 16) | (flags << 12) | (disp & 0xFFF);
        objStr = malloc(7);
        sprintf(objStr, "%06X", instr);
    } else {
        // format == 4: 8자리 16진수
        instr = (finalOpcode << 24) | (flags << 20) | (disp & 0xFFFFF);
        objStr = malloc(9);
        sprintf(objStr, "%08X", instr);
    }
    return objStr;
}

// format 4인 경우 M 레코드
char** generate_modification_records(token* t, int* count) {
    // 토큰이나 operand가 없으면 NULL 리턴
    if (!t || !t->operand[0]) return NULL;

    // 수정할 주소는 명령어 시작 주소 + 1 (relative expression은 아님)
    int mod_addr = t->addr + 1;

    // M 레코드 최대 10개 가정
    char** mods = malloc(sizeof(char*) * 10);
    *count = 0;

    // WORD directive의 relative expression 처리
        if (!strcasecmp(t->operator, "WORD")) {
        char expr[64];
        strcpy(expr, t->operand[0]);
        char *p;
        // ‘-’ 연산자 처리
        if ((p = strchr(expr, '-'))) {
            *p = '\0';
            char *sym1 = expr;
            char *sym2 = p + 1;
            char buf[32];
            sprintf(buf, "M %06X 06 +%s", mod_addr-1, sym1);
            mods[(*count)++] = strdup(buf);
            sprintf(buf, "M %06X 06 -%s", mod_addr-1, sym2);
            mods[(*count)++] = strdup(buf);
        }
        // ‘+’ 연산자 처리
        else if ((p = strchr(expr, '+'))) {
            *p = '\0';
            char *sym1 = expr;
            char *sym2 = p + 1;
            char buf[32];
            sprintf(buf, "M%06X06+%s", mod_addr-1, sym1);
            mods[(*count)++] = strdup(buf);
            sprintf(buf, "M%06X06+%s", mod_addr-1, sym2);
            mods[(*count)++] = strdup(buf);
        }
        // 단일 심볼
        else {
            char buf[32];
            sprintf(buf, "M%06X06+%s", mod_addr, expr);
            mods[(*count)++] = strdup(buf);
        }
        return mods;
    }

    char expr[64];
    strcpy(expr, t->operand[0]);

    // 오퍼랜드 문자열 복사 및 X 제거 (인덱스 주소 지정이 있으면 오퍼랜드만 추출)
    char* comma = strstr(expr, ",X");
    if (comma) *comma = '\0';

    // parsing 시작
    char *p = expr;
    char op[32];
    char sign = '+'; // 처음은 +

    while (*p) {
        // operator 추출
        if (*p == '+' || *p == '-') {
            sign = *p;
            p++;
            continue;
        }

        // operand 추출
        sscanf(p, "%31[^+-]", op);
        p += strlen(op);    // 다음 연산자로 이동

        // 외부 심볼이면 M 레코드 생성
        if (is_extref(op)) {
            mods[*count] = malloc(32);
            sprintf(mods[*count], "M %06X 05 %c%s", mod_addr, sign, op);
            (*count)++;
        }
    }

    return mods;
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
// Pass 2: Object Code 생성 및 H/D/R/T/M/E 레코드 출력
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

    int sectionCount = 0;

    // 각 control section 별로 object code를 생성함.
    // token_table의 순서대로 섹션이 연속된다고 가정하고 처리
    int i = 0;
    int sec = 1;
    while (i < token_line) {
        token *sectToken = token_table[i];

        // 프로그램 종료
        if (!strcasecmp(sectToken->operator, "END"))
            break;
        
        sectionCount++;

        // 섹션별 외부 참조 테이블 초기화
        extref_count = 0;

        // 해당 섹션의 시작 인덱스
        int sectStartIdx = i;

        // 다음 섹션 경계 찾기
        int endIdx = sectStartIdx + 1;
        while (endIdx < token_line &&
               strcasecmp(token_table[endIdx]->operator, "CSECT") &&
               strcasecmp(token_table[endIdx]->operator, "END")) {
            endIdx++;
        }

        int secStart = 0;   // 섹션이 시작하면 항상 주소 초기화
        int secLength = 0;

        char progName[7] = {0}; 
        if (strlen(sectToken->label) > 0) {
            // CSECT 또는 START 이후의 레이블만 프로그램 이름으로
            strncpy(progName, sectToken->label, 6);
        }

        // H Rec
        for (int k = sectStartIdx + 1; k < endIdx; k++) {
            token *t = token_table[k];
            if (t->comment[0] == '.') continue;
            if (!t->operator[0] ||
                !strcasecmp(t->operator, "START") ||
                !strcasecmp(t->operator, "CSECT") ||
                !strcasecmp(t->operator, "END")   ||
                !strcasecmp(t->operator, "EQU")   ||
                !strcasecmp(t->operator, "EXTDEF")||
                !strcasecmp(t->operator, "EXTREF")||
                !strcasecmp(t->operator, "LTORG"))
                continue;

            char *obj = generate_object_code(t);
            if (obj && *obj) {
                int len = strlen(obj) / 2;
                int addr = locctr_table[k];     // 이 addrdms 0부터 카운트된 섹션 ㄴ 상대주소
                if (addr + len > secLength)
                    secLength = addr + len;
            }
            free(obj);
        }

        fprintf(fp, "H %-6s %06X %06X\n", progName, secStart, section_length[sectionCount]);

        // D, R 레코드 생성
        char dRecord[256] = {0};
        char rRecord[256] = {0};
        for (int k = sectStartIdx; k < endIdx; k++) {
            token *t = token_table[k];
            if (!strcasecmp(t->operator, "EXTDEF")) {
                char *operand_copy = strdup(t->operand[0]);
                char *sym = strtok(t->operand[0], ",");
                while (sym) {
                    // sym_table에서 같은 섹션(currentSectionCount)와 같이 이름이 일치하는 addr 검색
                    unsigned int addr = 0;
                    for (int s = 0; s < label_num; s++) {
                        if (!strcmp(sym_table[s].symbol, sym) 
                            && sym_table[s].section == sectionCount) {
                            addr = sym_table[s].addr;
                            break;
                        }
                    }
                    char tmp[32];
                    // %-6s: 이름, %06X: 6자리 16진수
                    sprintf(tmp, "%-6s %06X ", sym, addr);
                    strcat(dRecord, tmp);
                    sym = strtok(NULL, ",");
                }
                free(operand_copy);
            }
            if (!strcasecmp(t->operator, "EXTREF")) {
                char *sym = strtok(t->operand[0], ",");
                while (sym) {
                    strcpy(extref_table[extref_count++], sym);
                    strcat(rRecord, sym);
                    strcat(rRecord, " ");
                    sym = strtok(NULL, ",");
                }
            }
        }
        if (dRecord[0]) fprintf(fp, "D %s\n", dRecord);
        if (rRecord[0]) fprintf(fp, "R %s\n", rRecord);

        // T, M 레코드 생성
        int tRecStart = -1, tRecLen = 0, modCount = 0;
        char tRecord[MAX_TEXT_RECORD_LENGTH * 2 + 1] = {0};
        char modRecords[100][32];

        
        // 섹션 내 모든 토큰 돌면서 T 레코드 축적 + M 레코드 모으기
        for (int k = sectStartIdx + 1; k < endIdx; k++) {
            token *t = token_table[k];

            // 1) 텍스트 레코드에 포함되지 않을 토큰(지시어·주석·RESW/RESB 등)은 건너뛴다
            if (!isTextRecordable(t)) continue;

            // 2) 객체 코드 생성
            char *obj = generate_object_code(t);
            if (!obj || *obj == '\0') {
                free(obj);
                continue;
            }
            int objBytes = strlen(obj) / 2;
            int addr     = locctr_table[k];

            // 3) 새 T 레코드 시작 주소 설정
            if (tRecLen == 0) {
                tRecStart = addr;
            }

            // 4) 길이 한도를 넘으면 기존 T 레코드 플러시
            if (tRecLen + objBytes > MAX_TEXT_RECORD_LENGTH) {
                fprintf(fp, "T %06X %02X %s\n", tRecStart, tRecLen, tRecord);
                tRecord[0] = '\0';
                tRecLen    = 0;
                tRecStart  = addr;
            }
            
            // 5) 객체 코드 누적
            strcat(tRecord, obj);
            tRecLen += objBytes;
            free(obj);

            // 6) format 4 명령어거나 WORD 디렉티브면 M 레코드도 모아두기
            if (t->operator[0] == '+' || !strcasecmp(t->operator, "WORD")) {
                int mcount = 0;
                char **mods = generate_modification_records(t, &mcount);
                for (int m = 0; m < mcount && modCount < 100; m++) {
                    strncpy(modRecords[modCount], mods[m], sizeof(modRecords[0]) - 1);
                    modRecords[modCount][sizeof(modRecords[0]) - 1] = '\0';
                    modCount++;
                    free(mods[m]);
                }
                free(mods);
            }
        }

        // 남은 T 레코드 출력
        if (tRecLen > 0) {
            fprintf(fp, "T %06X %02X %s\n", tRecStart, tRecLen, tRecord);
        }

        // 리터럴 풀 처리: LTORG/END 시점 객체
        for (int j = literalPoolStartSec[sec]; j < literalPoolEndSec[sec]; j++) {
            int relAddr = literal_table[j].addr - sectionStartAddr[sec];
            char litValue[32] = {0};
            extract_literal(literal_table[j].symbol, litValue);

            char *litObj;
            if (toupper(literal_table[j].symbol[1]) == 'C') {
                int len = strlen(litValue);
                litObj = malloc(len*2 + 1);
                litObj[0] = '\0';
                for (int k = 0; k < len; k++) {
                    char hx[3]; sprintf(hx, "%02X", (unsigned char)litValue[k]);
                    strcat(litObj, hx);
                }
            } else {
                litObj = strdup(litValue);
            }

            int litBytes = strlen(litObj) / 2;
            fprintf(fp, "T %06X %02X %s\n", relAddr, litBytes, litObj);
            free(litObj);
        }


        // 모아놓은 모든 M 레코드 순서대로 출력
        for (int m = 0; m < modCount; m++) {
            fprintf(fp, "%s\n", modRecords[m]);
        }

        // E 레코드 출력
        if (i == 0)
            fprintf(fp, "E %06X\n\n", secStart);
        else 
            fprintf(fp, "E\n\n");

        // 다음 섹션으로 이동
        i = endIdx;
        sec++;
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