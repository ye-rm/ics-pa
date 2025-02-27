/***************************************************************************************
 * Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#define BUF_SIZE 65536
#define MAX_DEPTH 8

// this should be enough
static char buf[BUF_SIZE] = {};
static char code_buf[BUF_SIZE + 128] = {}; // a little larger than `buf`
unsigned cur = 0;
static char *code_format =
    "#include <stdio.h>\n"
    "int main() { "
    "  unsigned result = %s; "
    "  printf(\"%%u\", result); "
    "  return 0; "
    "}";

//[min,max)
unsigned gen_random_num(int min, int max)
{
  return rand() % (max - min) + min;
}

void gen_num()
{
  // first num should not be zero
  buf[cur++] = gen_random_num('1', '9' + 1);
  for (int i = 0; i < gen_random_num(1, 6); i++)
  {
    buf[cur++] = gen_random_num('0', '9' + 1);
  }
}

void gen(char c)
{
  buf[cur++] = c;
}

void gen_rand_op()
{
  switch (gen_random_num(0, 4))
  {
  case 0:
    buf[cur++] = '+';
    break;
  case 1:
    buf[cur++] = '-';
    break;
  case 2:
    buf[cur++] = '/';
    break;
  default:
    buf[cur++] = '*';
    break;
  }
}

static void gen_rand_expr(int depth)
{
  if (depth > MAX_DEPTH){
    gen_num();
    return;
  }
  switch (gen_random_num(0, 3))
  {
  case 0:
    gen_num();
    break;
  case 1:
    gen('(');
    gen_rand_expr(depth + 1);
    gen(')');
    break;
  default:
    gen_rand_expr(depth + 1);
    gen_rand_op();
    if (buf[cur - 1] == '/')
      gen_num();
    else
      gen_rand_expr(depth + 1);
    break;
  }
}

int main(int argc, char *argv[])
{
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1)
  {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i++)
  {
    memset(buf, '\0', sizeof(buf[0]) * BUF_SIZE);
    gen_rand_expr(0);
    buf[cur++] = '\0';
    cur = 0;
    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0)
      continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    unsigned int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);
    printf("%u %s\n", result, buf);
  }
  return 0;
}
