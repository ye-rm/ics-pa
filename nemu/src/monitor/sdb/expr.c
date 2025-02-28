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

#include <isa.h>
#include <memory/vaddr.h>
#define TOKEN_LIMIT 65535
/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

enum
{
  TK_NOTYPE = 256,
  TK_NUM,
  TK_HEX,
  TK_REG,
  TK_LEFTBRACE,
  TK_RIGHTBRACE,
  TK_AND,
  TK_EQ,
  TK_NEQ = 263 ,
  TK_ADD,
  TK_MIN = 264,
  TK_DIV,
  TK_MULTI = 265
};

static struct rule
{
  const char *regex;
  int token_type;
} rules[] = {

    {" +", TK_NOTYPE}, // spaces
    {"\\+", TK_ADD},   // plus
    {"==", TK_EQ},     // equal
    {"!=", TK_NEQ},
    {"0x[0-9a-fA-F]", TK_HEX},
    {"\\$[a-zA-Z0-9]{2}", TK_REG},
    {"&&", TK_AND},
    {"\\-", TK_MIN},
    {"\\*", TK_MULTI},
    {"/", TK_DIV},
    {"[0-9]+", TK_NUM},
    {"\\(", TK_LEFTBRACE},
    {"\\)", TK_RIGHTBRACE},
};

#define NR_REGEX ARRLEN(rules)
static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex()
{
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++)
  {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0)
    {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token
{
  int type;
  char str[32];
} Token;

static Token tokens[TOKEN_LIMIT] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;
int token_idx = 0;
static bool make_token(char *e)
{
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0')
  {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++)
    {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0)
      {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        switch (rules[i].token_type)
        {
        case TK_NOTYPE:
          break;
        default:
          if (token_idx >= TOKEN_LIMIT)
          {
            Log("too many tokens(%d+)", TOKEN_LIMIT);
            return false;
          }
          if (substr_len > 32)
          {
            Log("token longer than 32");
            return false;
          }
          strncpy(tokens[token_idx].str, substr_start, substr_len);
          tokens[token_idx].str[substr_len] = '\0';
          tokens[token_idx++].type = rules[i].token_type;
        }
        break;
      }
    }

    if (i == NR_REGEX)
    {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

bool check_parentheses(int p, int q)
{
  int cur = 0;
  if (tokens[p].type == TK_LEFTBRACE && tokens[q].type == TK_RIGHTBRACE)
  {
    p++;
    q--;
  }
  else
    return false;
  for (int i = p; i <= q; i++)
  {
    if (tokens[i].type == TK_LEFTBRACE)
      cur++;
    else if (tokens[i].type == TK_RIGHTBRACE)
      cur--;
    if (cur < 0)
      return false;
  }
  if (cur == 0)
    return true;
  else
    return false;
}

bool is_op(int idx){
  switch (idx)
  {
  case TK_ADD:
  case TK_MULTI:
  case TK_EQ:
  case TK_AND: 
    return true;
  default:
    return false;
  }
}

int get_op(int p, int q)
{
  int op = p;
  int pri = TK_DIV;
  int in_brace = 0;
  for (int i = p; i <= q; i++)
  {
    if (tokens[i].type == TK_LEFTBRACE)
      in_brace++;
    else if (tokens[i].type == TK_RIGHTBRACE)
      in_brace--;
    if (in_brace == 0&&is_op(i)&&tokens[i].type<=pri)
    {
        op = i;
        pri = tokens[i].type;
    }
  }
  // Log("select op %s at token no. %d", tokens[op].str, op + 1);
  return op;
}

bool is_single_op(int op_idx)
{
  if (op_idx == 0)
    return true;
  int pre_type = tokens[op_idx - 1].type;
  switch (pre_type)
  {
  case TK_RIGHTBRACE:
  case TK_NUM:
  case TK_HEX:
  case TK_REG:
    return false;
  default:
    return true;
  }
}

uint32_t eval(int p, int q)
{
  if (p > q)
  {
    // Log("bad expression, may cause wrong result");
    return 0;
  }
  else if (p == q)
  {
    bool success = false;
    switch (tokens[p].type)
    {
    case TK_NUM:
      return atoi(tokens[p].str);
    default:
      unsigned reg = isa_reg_str2val(tokens[p].str+1,&success);
      if(success)
        return reg;
      else{
        Log("unable to find register %s",tokens[p].str);
        return 0;
      }
    }
  }
  else if (check_parentheses(p, q) == true)
  {
    return eval(p + 1, q - 1);
  }
  else
  {
    int op = get_op(p, q);
    int val_1 = eval(p, op - 1);
    int val_2 = eval(op + 1, q);
    Log("calculate %u %s %u ",val_1,tokens[op].str,val_2);
    if (is_single_op(op))
    {
      switch (tokens[op].str[0])
      {
      case '+':
        return val_2;
      case '-':
        return -val_2;
      case '*':
        return vaddr_read(val_2, 4);
      default:
        Log("bad single op :%s", tokens[op].str);
        return 0;
      }
    }
    switch (tokens[op].str[0])
    {
    case '+':
      return val_1 + val_2;
    case '-':
      return val_1 - val_2;
    case '*':
      return val_1 * val_2;
    case '/':
      if (val_2 == 0)
      {
        Log("divided by zero! invalid result");
        return val_1;
      }
      return val_1 / val_2;
    case '=':
      return val_1 == val_2;
    case '!':
      return val_1 != val_2;
    case '&':
      return val_1 && val_2;
    default:
      Log("bad op, may cause wrong result");
      return 0;
    }
  }
}

word_t expr(char *e, bool *success)
{
  if (!make_token(e))
  {
    *success = false;
    return 0;
  }
  Log("make token succeed");
  uint32_t ret = eval(0, token_idx - 1);
  token_idx = 0;
  *success = true;
  return ret;
}
