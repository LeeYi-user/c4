// c4.c - C in four functions // 用 4 個函數寫出 C 語言

// char, int, and pointer types // 只支援字元、整數和指標型態
// if, while, return, and expression statements // 語法上則支援 if, while, return 和表達式語句
// just enough features to allow self-compilation and a bit more // 剛好有足夠的功能來進行自編譯和其他事

// Written by Robert Swierczek

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#define int long long

char *p, *lp, // current position in source code (p: 目前原始碼指標, lp: 上一行原始碼指標)
     *data;   // data/bss pointer (資料段機器碼指標)

int *e, *le,  // current position in emitted code (e: 目前機器碼指標, le: 上一行機器碼指標)
    *id,      // currently parsed identifier (id: 目前的 id)
    *sym,     // symbol table (simple list of identifiers) (符號表)
    tk,       // current token (目前 token)
    ival,     // current token value (目前的 token 值)
    ty,       // current expression type (目前的運算式型態)
    loc,      // local variable offset (區域變數的位移)
    line,     // current line number (目前行號)
    src,      // print source and assembly flag (印出原始碼)
    debug;    // print executed instructions (印出執行指令 -- 除錯模式)

// tokens and classes (operators last and in precedence order) (按優先權順序排列)
enum { // token : 0-127 直接用該字母表達， 128 以後用代號。
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// opcodes (操作碼)
enum { LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT };

// types (支援型態，只有 int, char, pointer)
enum { CHAR, INT, PTR };

// 因為沒有 struct，所以使用 offset 代替，例如 id[Tk] 代表 id.Tk (token), id[Hash] 代表 id.Hash, id[Name] 代表 id.Name, .....
// identifier offsets (since we can't create an ident struct)
// Symbol table entry's field indexes, except for `Idsz`. // 除了 `Idsz` 之外的符號表條目的字段索引
// `Hash`: Symbol name's hash value. // 符號名稱的雜湊值
// `Name`: Symbol name's string address. // 符號名稱的字串位址
// `Class`: Symbol type: // 符號類型
// - Num: Enum name. // 列舉名稱
// - Fun: Function name. // 函數名稱
// - Sys: System call name. // 系統呼叫名稱
// - Glo: Global variable name. // 全域變數名稱
// - Loc: Local variable name. // 區域變數名稱
// `Type`: Associated value type. e.g. `CHAR`, `INT`. // 關聯值型態 (例: 字元、整數)
// `Val`: Associated value. // 關聯值
// `HClass`: Backup field for `Class` field. // `Class` 字段的備用字段
// `HType`: Backup field for `Type` field. // `Type` 字段的備用字段
// `HVal`: Backup field for `Val` field. // `Val` 字段的備用字段
// `Idsz`: Symbol table entry size. // `Idsz` 字段的備用字段
enum { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz };

// Read token. // 讀取 token
void next() // 詞彙解析 lexer
{
  char *pp;

  // Get current character. // 取得當前字元
  // While current character is not `\0`. // 當當前字元不為空字元
  // The source code has been read into source code buffer and ended with `\0`. // 源代碼已被讀入其緩衝區, 並以空字元結束
  while (tk = *p) {
    // Point to next character. // 指向下一個字元
    ++p;

    // If current character is newline. // 如果新字元是換行字元
    if (tk == '\n') {
      // If switch for printing source code line and corresponding instructions
      // is on. // 如果有用到 c4 的 -s 參數
      if (src) {
        // Print source code line. // 印出源代碼行
        printf("%d: %.*s", line, p - lp, lp);

        // Point `lp` to the last newline. // 將 `lp` 指向新一行的原始碼開頭
        lp = p;

        // While have instruction to print. // 當有指令要印
        while (le < e) {
          // Print opcode. // 印出操作碼
          printf("%8.4s", &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
                           "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                           "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[*++le * 5]);

          // If the opcode <= ADJ, it has operand. // 當操作碼為 LEA、IMM、JMP、JSR、BZ、BNZ、ENT、ADJ 其中之一, 則他有運算元
          // Print operand. // 印出運算元
          if (*le <= ADJ) printf(" %d\n", *++le); else printf("\n");
        }
      }

      // Increment line number. // 增加行號
      ++line;
    }
    // If current character is `#`, it is preprocessing directive. // 如果當前字元為井字號, 則他是預處理指令
    // Preprocessing directive is ignored. // 預處理指令是被忽略的
    else if (tk == '#') {
      // While current character is not `\0` and current character is not
      // newline. // 當當前字元不是空字元也不是換行字元
      // Skip current character. // 跳過該字元
      while (*p != 0 && *p != '\n') ++p;
    }
    // If current character is letter or underscore, it is identifier. // 如果當前字元為字母或下劃線, 則他是一個變數名稱
    else if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || tk == '_') {
      // Point `pp` to the first character. // 將 `pp` 指向第一個字元
      pp = p - 1;

      // While current character is letter, digit, or underscore. // 當當前字元為字母、數字或下劃線
      while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
        // Use current character to compute hash value. // 使用當前字元來計算雜湊值
        tk = tk * 147 + *p++;

      // Combine the hash value with string length. // 將雜湊值與字串長度結合在一起
      tk = (tk << 6) + (p - pp);

      // Point `id` to symbol table. // 將 `id` 指向符號表
      id = sym;

      // While current symbol table entry is in use. // 當當前的符號表條目正在使用中
      while (id[Tk]) {
        // 如果 token 的雜湊值和名稱, 跟當前符號表條目的雜湊值和名稱相等
        // If current symbol table entry's hash is equal and name is equal, it
        // means the name has been seen before. // 則表示該變數名稱已經見過
        // Set token type be the entry's token type. // 將 token 類型設定為該變數的 token 類型, 然後直接 return
        if (tk == id[Hash] && !memcmp((char *)id[Name], pp, p - pp)) { tk = id[Tk]; return; }

        // Point to next table entry. // 不然就指向下一個符號表條目
        id = id + Idsz;
      }

      // At this point, existing symbol name is not found. // 到了這裡, 則表示找不到該變數名稱
      // `id` is pointing to the first unused symbol table entry. // 將 `id` 指向到第一個未使用的符號表條目

      // Store the name's string address. // 儲存變數名稱的字串位址
      id[Name] = (int)pp;

      // Store the name's hash value. // 儲存變數名稱的雜湊值
      id[Hash] = tk;

      // Set token type. // 設定 token 類型
      tk = id[Tk] = Id;

      return;
    }
    // If current character is digit, it is number constant. // 如果當前字元是數字, 則他是一個常數
    else if (tk >= '0' && tk <= '9') {
      // If current character is not `0`, it is decimal notation. // 如果當前字元不是 0, 則他是十進位
      // Convert decimal notation to value. // 將十進位轉換成值
      if (ival = tk - '0') { while (*p >= '0' && *p <= '9') ival = ival * 10 + *p++ - '0'; }
      // If current character is `0` and following character is `x` or
      // `X`, it is hexadecimal notation. // 如果當前字元是 0, 且下一個字元是 x, 則他是十六進位
      else if (*p == 'x' || *p == 'X') {
        // Convert hexadecimal notation to value. // 將十六進位轉換成值
        while ((tk = *++p) && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F')))
          ival = ival * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
      }
      // If current character is `0` and following character is not `x` or // 如果當前字元是 0, 且下一個字元不是 x
      // `X`, it is octal notation. // 則他是八進位
      // Convert octal notation to value. // 將八進位轉換成值
      else { while (*p >= '0' && *p <= '7') ival = ival * 8 + *p++ - '0'; }

      // Set token type. // 設定 token 類型
      tk = Num;

      return;
    }
    // If current character is `/`, it is comments or division operator. // 如果當前字元是正斜線, 則他是註解或除法運算符
    else if (tk == '/') {
      // If following character is `/`, it is comments. // 如果下一個字元還是正斜線, 則他是註解
      if (*p == '/') {
        // Point to next character. // 指向到下一個字元
        ++p;

        // While current character is not `\0` and current character is not
        // newline. // 當當前字元不是空字元也不是換行字元
        // Skip current character. // 跳過當前字元
        while (*p != 0 && *p != '\n') ++p;
      }
      // If following character is not `/`, it is division operator. // 如果下一個字元不是正斜線, 則他是除法運算符
      else {
        // Set token type. // 設定 token 類型
        tk = Div;

        return;
      }
    }
    // If current character is `'` or `"`, it is character constant or string // 如果當前字元是引號, 則他是字元或字串
    // constant.
    else if (tk == '\'' || tk == '"') {
      // Store data buffer's current location. // 儲存資料段的當前位置
      pp = data;

      // While current character is not `\0` and current character is not the
      // quote character. // 當當前字元不是空字元且不是引號
      while (*p != 0 && *p != tk) {
        // If current character is `\`, it is escape notation or simply `\`
        // character. // 如果當前字元是反斜線, 則他是跳脫字元或是單純的反斜線
        if ((ival = *p++) == '\\') {
          // If following character is `n`, it is newline escape, // 如果下一個字元是 n, 則他是一個跳脫/換行字元
          if ((ival = *p++) == 'n') ival = '\n';
        }

        // If it is string constant, copy current character to data buffer. // 如果他是一個字串, 就將當前字元複製到資料段裡
        if (tk == '"') *data++ = ival;
      }

      // Point to next character. // 指向下一個字元
      ++p;

      // If it is string constant, use the string's address as the token's // 如果他是字串
      // associated value. The token type is `"`. // 就將字串位址作為 token 的關聯值, 並把 token 類型設定為雙引號
      // If it is character constant, use the character's value as the token's // 如果他是字元
      // associated value. Set token type be number constant. // 就將字元值作為 token 的關聯值, 並把 token 類型設定為數字
      if (tk == '"') ival = (int)pp; else tk = Num;

      return;
    }
    else if (tk == '=') { if (*p == '=') { ++p; tk = Eq; } else tk = Assign; return; }
    else if (tk == '+') { if (*p == '+') { ++p; tk = Inc; } else tk = Add; return; }
    else if (tk == '-') { if (*p == '-') { ++p; tk = Dec; } else tk = Sub; return; }
    else if (tk == '!') { if (*p == '=') { ++p; tk = Ne; } return; }
    else if (tk == '<') { if (*p == '=') { ++p; tk = Le; } else if (*p == '<') { ++p; tk = Shl; } else tk = Lt; return; }
    else if (tk == '>') { if (*p == '=') { ++p; tk = Ge; } else if (*p == '>') { ++p; tk = Shr; } else tk = Gt; return; }
    else if (tk == '|') { if (*p == '|') { ++p; tk = Lor; } else tk = Or; return; }
    else if (tk == '&') { if (*p == '&') { ++p; tk = Lan; } else tk = And; return; }
    else if (tk == '^') { tk = Xor; return; }
    else if (tk == '%') { tk = Mod; return; }
    else if (tk == '*') { tk = Mul; return; }
    else if (tk == '[') { tk = Brak; return; }
    else if (tk == '?') { tk = Cond; return; }
    else if (tk == '~' || tk == ';' || tk == '{' || tk == '}' || tk == '(' || tk == ')' || tk == ']' || tk == ',' || tk == ':') return;
  }
}

// Parse expression. // 運算式 expression, 其中 lev 代表優先等級
// `lev`: Current operator precedence. Greater value means higher precedence.
// Operator precedence (lower first): // 運算符優先級 (從低到高)
// Assign  =
// Cond    ?
// Lor     ||
// Lan     &&
// Or      |
// Xor     ^
// And     &
// Eq      ==
// Ne      !=
// Lt      <
// Gt      >
// Le      <=
// Ge      >=
// Shl     <<
// Shr     >>
// Add     +
// Sub     -
// Mul     *
// Div     /
// Mod     %
// Inc     ++
// Dec     --
// Brak    [
void expr(int lev)
{
  int t, *d;

  // If current token is input end, print error and exit program. // 如果當前 token 是檔案結尾, 則印出錯誤並退出程序
  if (!tk) { printf("%d: unexpected eof in expression\n", line); exit(-1); }

  // If current token is number constant. // 如果當前 token 是數字
  // Add `IMM` instruction to load the number's value to register. // 添加 `IMM` 指令來將數字的值載入到暫存器中
  // Set result value type be `INT`. // 將結果值型態設為整數
  else if (tk == Num) { *++e = IMM; *++e = ival; next(); ty = INT; }
  // If current token is string constant. // 如果當前 token 是字串
  else if (tk == '"') {
    // Add `IMM` instruction to load the string's address to register. // 添加 `IMM` 指令來將字串位址載入到暫存器中
    // Read token. // 讀取 token
    *++e = IMM; *++e = ival; next();

    // While current token is string constant, it is adjacent string
    // constants, e.g. "abc" "def". // 當當前 token 是字串, 則他是一個相鄰字串 (例: "abc""def")
    // In `next`, the string's characters have been copied to data buffer. // 在 next() 函數中, 字串中的字元都已經被複製到資料段裡
    // This implements concatenation of adjacent string constants. // 這裡用來實現相鄰字串的連接
    // Read token. // 讀取 token
    while (tk == '"') next();

    // Point `data` to next int-aligned address. // 將整數作為大小對齊, 並將 `data` 指向下一個對齊的位址
    // E.g. `-sizeof(int)` is -4, i.e. 0b11111100. // 例: `-sizeof(int)` 是 -4, 也就是 0b11111100
    // This guarantees to leave at least one '\0' after the string.
    // 這保證在字串後至少留下一個空字元
    // Set result value type be char pointer. // 將結果值型態設為字元指標
    // CHAR + PTR = PTR because CHAR is 0. // 因為 CHAR 是 0, 所以 CHAR + PTR 等於 PTR
    data = (char *)((int)data + sizeof(int) & -sizeof(int)); ty = PTR;
  }
  // If current token is `sizeof` operator. // 如果當前 token 是 `sizeof` 運算符
  else if (tk == Sizeof) {
    // Read token. // 讀取token
    // If current token is `(`, read token, else print error and exit // 如果當前 token 是左圓括號, 讀取 token
    // program. // 否則印出錯誤並退出程序
    next(); if (tk == '(') next(); else { printf("%d: open paren expected in sizeof\n", line); exit(-1); }

    // Set operand value type be `INT`. // 將運算元型態設為整數
    // If current token is `int`, read token. // 如果當前 token 是 `int`, 讀取 token
    // If current token is `char`, read token, set operand value type be // 如果當前 token 是 `char`, 讀取 token
    // `CHAR`. // 然後將運算元型態設為字元
    ty = INT; if (tk == Int) next(); else if (tk == Char) { next(); ty = CHAR; }

    // While current token is `*`, it is pointer type. // 如果當前 token 是星號, 則他是指標型態
    // Add `PTR` to the operand value type. // 添加 `PTR` 到運算元型態
    while (tk == Mul) { next(); ty = ty + PTR; }

    // If current token is `)`, read token, else print error and exit program. // 如果當前 token 是右圓括號, 讀取 token
    // 否則印出錯誤並退出程序
    if (tk == ')') next(); else { printf("%d: close paren expected in sizeof\n", line); exit(-1); }

    // Add `IMM` instruction to load the operand value's size to register. // 添加 `IMM` 指令來將運算元大小載入到暫存器中
    *++e = IMM; *++e = (ty == CHAR) ? sizeof(char) : sizeof(int);

    // Set result value type be `INT`. // 將結果值型態設為整數
    ty = INT;
  }
  // If current token is identifier. // 如果當前 token 是變數名稱
  else if (tk == Id) {
    // Store the identifier's symbol table entry address. // 儲存該變數名稱的符號表條目位址
    // Read token. // 讀取 token
    d = id; next();

    // If current token is `(`, it is function call. // 如果當前 token 是左圓括號, 他就是函數呼叫
    if (tk == '(') {
      // Read token. // 讀取 token
      next();

      // Arguments count. // 參數數量
      t = 0;

      // While current token is not `)`. // 當當前 token 不是右圓括號
      // Parse argument expression. // 解析參數表達式
      // Add `PSH` instruction to push the argument to stack. // 添加 `PSH` 指令來將參數推進堆疊
      // Increment arguments count. // 增加參數計數
      // If current token is `,`, skip. // 如我當前 token 是逗號, 就跳過
      while (tk != ')') { expr(Assign); *++e = PSH; ++t; if (tk == ',') next(); }

      // Skip `)` // 跳過右圓括號
      next();

      // If it is system call, // 如果他是系統呼叫
      // add the system call's opcode to instruction buffer. // 添加系統呼叫的操作碼到指令緩衝區
      if (d[Class] == Sys) *++e = d[Val];
      // If it is function call, // 如果他是函數呼叫
      // add `JSR` opcode and the function address to instruction buffer. // 添加 `JSR` 操作碼和函數位址到指令緩衝區
      else if (d[Class] == Fun) { *++e = JSR; *++e = d[Val]; }
      // Else print error message and exit program. // 否則印出錯誤訊息並退出程序
      else { printf("%d: bad function call\n", line); exit(-1); }

      // If have arguments. // 如果有參數
      // Add `ADJ` instruction and arguments count to instruction buffer to // 添加 `ADJ` 指令和參數計數到指令緩衝區
      // pop arguments off stack after returning from function call. // 以便在從函數呼叫返回後將參數從堆疊中彈出
      if (t) { *++e = ADJ; *++e = t; }

      // Set result value type be the system call or function's return type. // 將結果值型態設為系統呼叫或函數的回傳型態
      ty = d[Type];
    }
    // If it is enum name. // 如果他是 enum 名稱
    // Add `IMM` instruction to load the enum value to register. // 添加 `IMM` 指令來將 enum 值載入到暫存器中
    // Set result value type be `INT`. // 將結果值型態設定為整數
    else if (d[Class] == Num) { *++e = IMM; *++e = d[Val]; ty = INT; }
    // If it is none of above, assume it is a variable name. // 如果他不是上述情況, 假設他是一個變數名稱
    else {
      // 6S71X // 這裡是原作者用來記錄位置的代碼, 不要上網查他...
      // If it is local variable, add `LEA` opcode and the local variable's // 如果他是區域變數, 則添加 `LEA` 指令
      // offset to instruction buffer to load the local variable's address to // 和從區域變數到指令緩衝區的偏移量
      // register. // 來將區域變數位址載入到暫存器
      if (d[Class] == Loc) { *++e = LEA; *++e = loc - d[Val]; }
      // If it is global variable, add `IMM` instruction to load the global // 如果他是全域變數, 則添加 `IMM` 指令
      // variable's address to register. // 來將全域變數位址載入到暫存器
      else if (d[Class] == Glo) { *++e = IMM; *++e = d[Val]; }
      // Else print error message and exit program. // 否則印出錯誤訊息並退出程序
      else { printf("%d: undefined variable\n", line); exit(-1); }

      // 2WQE9 // 這裡是原作者用來記錄位置的代碼, 不要上網查他...
      // Add `LC`/`LI` instruction to load the value on the address in register
      // to register. // 添加 `LC`/`LI` 指令來將暫存器中的位址所指向的值載入到暫存器
      *++e = ((ty = d[Type]) == CHAR) ? LC : LI;
    }
  }
  // 如果當前 token 是左圓括號, 則他是圓括號中的強制轉換或表達式
  // If current token is `(`, it is cast or expression in parentheses.
  else if (tk == '(') {
    // Read token. // 讀取 token
    next();

    // If current token is `int` or `char`, it is cast. // 如果當前 token 是 `int` 或 `char`, 則他是強制轉換
    if (tk == Int || tk == Char) {
      // Get the cast's base data type. // 取得強制轉換的基本資料型態
      // Read token. // 讀取 token
      t = (tk == Int) ? INT : CHAR; next();

      // While current token is `*`, it is pointer type. // 如果當前 token 是星號, 則他是指標型態
      // Add `PTR` to the cast's data type. // 添加 `PRT` 到強制轉換的資料型態
      while (tk == Mul) { next(); t = t + PTR; }

      // If current token is not `)`, print error and exit program. // 如果當前 token 不是右圓括號, 印出錯誤並退出程序
      if (tk == ')') next(); else { printf("%d: bad cast\n", line); exit(-1); }

      // Parse casted expression. // 解析強制轉換表達式
      // Use `Inc` to allow only `++`, `--`, `[]` operators in the expression.
      expr(Inc); // 在表達式中使用 `Inc` 以限制只能使用 `++`, `--`, `[]` 運算符

      // Set result value type be the cast's data type. // 將結果值型態設為強制轉換的資料型態
      ty = t;
    }
    // If current token is not `int` or `char`, it is expression in
    // parentheses. // 如果當前 token 不是 `int` 或 `char`, 則他是圓括號中的表達式
    else {
      // Parse expression. // 解析表達式
      expr(Assign);

      // If current token is not `)`, print error and exit program. // 如果當前 token 不是右圓括號, 印出錯誤並退出程序
      if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    }
  }
  // If current token is `*`, it is dereference operator. // 如果當前 token 是星號, 則他是解引用運算符
  else if (tk == Mul) {
    // Read token. // 讀取 token
    // Parse operand expression. // 解析運算元表達式
    // Use `Inc` to allow only `++`, `--`, `[]` operators in the expression.
    next(); expr(Inc); // 在表達式中使用 `Inc` 以限制只能使用 `++`, `--`, `[]` 運算符

    // If operand value type is not pointer, print error and exit program.
    // 如果運算元型態不是指標, 則印出錯誤並退出程序
    if (ty > INT) ty = ty - PTR; else { printf("%d: bad dereference\n", line); exit(-1); }

    // Add `LC`/`LI` instruction to load the value on the address in register
    // to register. // 添加 `LC`/`LI` 指令來將暫存器中的位址所指向的值載入到暫存器
    *++e = (ty == CHAR) ? LC : LI;
  }
  // If current token is `&`, it is address-of operator. // 如果當前 token 是 and 號, 則他是傳址運算符
  else if (tk == And) {
    // Read token. // 讀取 token
    // Parse operand expression. // 解析運算元表達式
    // Use `Inc` to allow only `++`, `--`, `[]` operators in the expression.
    next(); expr(Inc); // 在表達式中使用 `Inc` 以限制只能使用 `++`, `--`, `[]` 運算符

    // The operand of the address-of operator should be a variable.
    // 傳址運算符的運算元應該是一個變數
    // The instructions to get the variable's address has been added at 6S71X.
    // 用來獲取變數位址的指令已經被添加到 6S71X 這個位置
    // Only need to remove the `LC`/`LI` instruction added at 2WQE9.
    // 只需要去掉在 2WQE9 處添加的 `LC`/`LI` 指令即可
    // If current instruction is `LC`/`LI`, remove it, else print error and
    // exit program. // 如果當前指令是 `LC`/`LI`, 移除他, 否則印出錯誤並退出程序
    if (*e == LC || *e == LI) --e; else { printf("%d: bad address-of\n", line); exit(-1); }
    // 將結果值型態設為指向當前值型態的指標
    // Set result value type be pointer to current value type.
    ty = ty + PTR;
  }
  // If current token is `!`, it is boolean negation operator. // 如果當前 token 是驚嘆號, 則他是布林運算符
  // Add instructions to compute `x == 0` because `!x` is equivalent to
  // `x == 0`. // 添加指令以計算 `x == 0`, 因為 `!x` 等價於 `x == 0`
  // Set result value type be `INT`. // 將結果值型態設為整數
  else if (tk == '!') { next(); expr(Inc); *++e = PSH; *++e = IMM; *++e = 0; *++e = EQ; ty = INT; }
  // If current token is `~`, it is bitwise inversion operator. // 如果當前 token 是波浪號, 則他是位元反轉運算符
  // Add instructions to compute `x ^ -1` because `~x` is equivalent to
  // `x ^ -1`. // 添加指令以計算 `x ^ -1`, 因為 `~x` 等價於 `x ^ -1`
  // Set result value type be `INT`. // 將結果值型態設為整數
  else if (tk == '~') { next(); expr(Inc); *++e = PSH; *++e = IMM; *++e = -1; *++e = XOR; ty = INT; }
  // If current token is `+`, it is unary addition operator. // 如果當前 token 是加號, 則他是一元加法運算符
  // Read token. // 讀取 token
  // Parse operand expression. // 解析運算元表達式
  // Set result value type be `INT`. // 將結果值型態設為整數
  else if (tk == Add) { next(); expr(Inc); ty = INT; }
  // If current token is `-`, it is unary subtraction operator. // 如果當前 token 是減號, 則他是一元減法運算符
  else if (tk == Sub) {
    // Read token. // 讀取 token
    // Add `IMM` instruction to load number constant's negated value or `-1`
    // to register. // 添加 `IMM` 指令來將數字的負值或 `-1` 載入到暫存器
    next(); *++e = IMM;

    // If operand is number constant, add negated value to instruction buffer.
    // 如果運算元是數字, 則將負值添加到指令緩衝區
    // If operand is not number constant, add `-1` to instruction buffer. Add
    // 如果運算元不是數字, 則將 `-1` 添加到指令緩衝區
    // `PSH` instruction to push `-1` in register to stack. Parse operand
    // 然後再添加 `PSH` 指令將 `-1` 推進堆疊
    // expression. Add `MUL` instruction to multiply `-1` on stack by the
    // 最後再解析運算元表達式, 添加 `MUL` 指令來將堆疊中的 `-1` 乘以暫存器中的運算元
    // operand value in register.
    if (tk == Num) { *++e = -ival; next(); } else { *++e = -1; *++e = PSH; expr(Inc); *++e = MUL; }

    // Set result value type be `INT`. // 將結果值型態設為整數
    ty = INT;
  }
  // 如果當前 token 是前置遞增或遞減運算符
  // If current token is prefix increment or decrement operator.
  else if (tk == Inc || tk == Dec) {
    // Store current token type. // 儲存當前 token 類型
    // Read token. // 讀取 token
    // Parse operand expression. // 解析運算元表達式
    t = tk; next(); expr(Inc);

    // If current instruction is `LC`, insert a `PSH` instruction before `LC`
    // 如果當前指令是 `LC`, 在 `LC` 之前插入一個 `PSH` 指令
    // to push variable address in register to stack for use by the `SC`
    // 來將暫存器中的變數位址推進堆疊, 供下面添加的 `SC` 指令使用
    // instruction added below.
    if (*e == LC) { *e = PSH; *++e = LC; }
    // If current instruction is `LI`, insert a `PSH` instruction before `LI`
    // 如果當前指令是 `LI`, 在 `LI` 之前插入一個 `PSH` 指令
    // to push variable address in register to stack for use by the `SI`
    // 來將暫存器中的變數位址推進堆疊, 供下面添加的 `SI` 指令使用
    // instruction added below.
    else if (*e == LI) { *e = PSH; *++e = LI; }
    // Else print error and exit program. // 否則印出錯誤並退出程序
    else { printf("%d: bad lvalue in pre-increment\n", line); exit(-1); }

    // Add `PSH` instruction to push operand value in register to stack
    // 添加 `PSH` 指令來將暫存器中的運算元推進堆疊
    // for use by the `ADD`/`SUB` instruction added below.
    // 供下面添加的 `ADD`/`SUB` 指令使用
    *++e = PSH;

    // Add `IMM` instruction to load increment/decrement value to register.
    // 添加 `IMM` 指令來將遞增/遞減值載入到暫存器
    *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);

    // Add `ADD`/`SUB` instruction to compute result value.
    // 添加 `ADD`/`SUB` 指令來計算結果值
    *++e = (t == Inc) ? ADD : SUB;

    // Add `SC`/`SI` instruction to save result value in register to address
    // held on stack. // 添加 `SC`/`SI` 指令來將暫存器中的結果值保存到堆疊中的位址
    *++e = (ty == CHAR) ? SC : SI;
  }
  // Else print error and exit program. // 否則印出錯誤並退出程序
  else { printf("%d: bad expression\n", line); exit(-1); }

  // While current token type is >= current operator precedence, // 當當前 token 類型大於等於當前運算符優先級
  // it is an operator that should be handled here. // 則他是一個應該在這裡處理的運算符
  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method // 遞迴下降運算符優先級解析器
    // Store current value type. // 儲存當前值類型
    t = ty;

    // If current token is assignment operator. // 如果當前 token 是賦值運算符 
    if (tk == Assign) {
      // Read token. // 讀取 token
      next();

      // If current instruction is `LC`/`LI`, current value in register is
      // 如果當前指令是 `LC`/`LI`, 則暫存器中的當前值是變數位址
      // variable address, replace current instruction with `PSH` instruction
      // 將當前指令替換為 `PSH` 指令來將變數位址推進堆疊
      // to push the variable address to stack for use by the `SC`/`SI`
      // instruction added below. // 供下面添加的 `SC`/`SI` 指令使用
      // If current instruction is not `LC`/`LI`, current value in register is
      // 如果當前指令不是 `LC`/`LI`, 則暫存器中的當前值不是變數位址
      // not variable address, print error and exit program.
      // 印出錯誤並退出程序
      if (*e == LC || *e == LI) *e = PSH; else { printf("%d: bad lvalue in assignment\n", line); exit(-1); }

      // Parse RHS expression. // 解析 RHS 表達式
      // Add `SC`/`SI` instruction to save value in register to variable
      // address held on stack. // 添加 `SC`/`SI` 指令來將暫存器中的值保存到堆疊中的變數位址
      expr(Assign); *++e = ((ty = t) == CHAR) ? SC : SI;
    }
    // If current token is conditional operator. // 如果當前 token 是條件運算符
    else if (tk == Cond) {
      // Read token. // 讀取 token
      next();

      // Add jump-if-zero instruction `BZ` to jump to false branch. // 添加條件跳轉指令 `BZ` 用來跳轉到假分支
      // Point `d` to the jump address field to be patched later. // 將 `d` 指向稍後需要修補的跳轉位址字段
      *++e = BZ; d = ++e;

      // Parse true branch's expression. // 解析真分支的表達式
      expr(Assign);

      // If current token is not `:`, print error and exit program. // 如果當前 token 不是冒號, 則印出錯誤並退出程序
      if (tk == ':') next(); else { printf("%d: conditional missing colon\n", line); exit(-1); }

      // Patch the jump address field pointed to by `d` to hold the address of
      // false branch. // 修補 `d` 指向的跳轉位址字段以保存錯誤分支的位址
      // `+ 3` counts the `JMP` instruction added below.
      // 加 3 計算下面添加的 `JMP` 指令
      // Add `JMP` instruction after the true branch to jump over the false
      // branch. // 在真分支後面添加 `JMP` 指令, 跳過假分支
      // Point `d` to the jump address field to be patched later.
      // 將 `d` 指向稍後需要修補的跳轉位址字段
      *d = (int)(e + 3); *++e = JMP; d = ++e;

      // Parse false branch's expression. // 解析假分支的表達式
      expr(Cond);

      // Patch the jump address field pointed to by `d` to hold the address
      // past the false branch. // 修補 `d` 指向的跳轉位址字段以保存經過錯誤分支的位址
      *d = (int)(e + 1);
    }
    // If current token is logical OR operator. // 如果當前 token 是邏輯或運算符
    // Read token. // 讀取 token
    // Add jump-if-nonzero instruction `BNZ` to implement short circuit.
    // 添加條件跳轉指令 `BNZ` 以實現短路運算
    // Point `d` to the jump address field to be patched later.
    // 將 `d` 指向稍後需要修補的跳轉位址字段
    // Parse RHS expression. // 解析 RHS 表達式
    // Patch the jump address field pointed to by `d` to hold the address past
    // the RHS expression. // 修補 `d` 指向的跳轉位址字段以保存經過 RHS 表達式的位址
    // Set result value type be `INT`. // 將結果值型態設為整數
    else if (tk == Lor) { next(); *++e = BNZ; d = ++e; expr(Lan); *d = (int)(e + 1); ty = INT; }
    // If current token is logical AND operator. // 如果當前 token 是邏輯與運算符
    // Read token. // 讀取 token
    // Add jump-if-zero instruction `BZ` to implement short circuit.
    // 添加條件跳轉指令 `BZ` 以實現短路運算
    // Point `d` to the jump address field to be patched later.
    // 將 `d` 指向稍後需要修補的跳轉位址字段
    // Parse RHS expression. // 解析 RHS 表達式
    // Patch the jump address field pointed to by `d` to hold the address past
    // the RHS expression. // 修補 `d` 指向的跳轉位址字段以保存經過 RHS 表達式的位址
    // Set result value type be `INT`. // 將結果值型態設為整數
    else if (tk == Lan) { next(); *++e = BZ;  d = ++e; expr(Or);  *d = (int)(e + 1); ty = INT; }
    // If current token is bitwise OR operator. // 如果當前 token 是位元或運算符
    // Read token. // 讀取 token
    // Add `PSH` instruction to push LHS value in register to stack.
    // 添加 `PSH` 指令來將暫存器中的 LHS 值推進堆疊
    // Parse RHS expression. // 解析 RHS 表達式
    // Add `OR` instruction to compute the result. // 添加 `OR` 指令來計算結果
    // Set result value type be `INT`. // 將結果值型態設為整數
    // The following operators are similar. // 下面的運算符也類似這樣
    else if (tk == Or)  { next(); *++e = PSH; expr(Xor); *++e = OR;  ty = INT; }
    else if (tk == Xor) { next(); *++e = PSH; expr(And); *++e = XOR; ty = INT; }
    else if (tk == And) { next(); *++e = PSH; expr(Eq);  *++e = AND; ty = INT; }
    else if (tk == Eq)  { next(); *++e = PSH; expr(Lt);  *++e = EQ;  ty = INT; }
    else if (tk == Ne)  { next(); *++e = PSH; expr(Lt);  *++e = NE;  ty = INT; }
    else if (tk == Lt)  { next(); *++e = PSH; expr(Shl); *++e = LT;  ty = INT; }
    else if (tk == Gt)  { next(); *++e = PSH; expr(Shl); *++e = GT;  ty = INT; }
    else if (tk == Le)  { next(); *++e = PSH; expr(Shl); *++e = LE;  ty = INT; }
    else if (tk == Ge)  { next(); *++e = PSH; expr(Shl); *++e = GE;  ty = INT; }
    else if (tk == Shl) { next(); *++e = PSH; expr(Add); *++e = SHL; ty = INT; }
    else if (tk == Shr) { next(); *++e = PSH; expr(Add); *++e = SHR; ty = INT; }
    // If current token is addition operator. // 如果當前 token 是加法運算符
    else if (tk == Add) {
      // Read token. // 讀取 token
      // Add `PSH` instruction to push LHS value in register to stack.
      // 添加 `PSH` 指令來將暫存器中的 LHS 值推進堆疊
      // Parse RHS expression. // 解析 RHS 表達式
      next(); *++e = PSH; expr(Mul);

      // If LHS value type is pointer, // 如果 LHS 值型態是指標
      // the RHS value should be multiplied by int size to get address offset.
      // 則 RHS 值應該乘以整數大小來獲得位址偏移量
      // Add `PSH` instruction to push RHS value in register to stack.
      // 添加 `PSH` 指令來將暫存器中的 RHS 值推進堆疊
      // Add `IMM` instruction to load int size to register.
      // 添加 `IMM` 指令來將整數大小載入到暫存器
      // Add `MUL` instruction to multiply RHS value on stack by int size in
      // register to get the address offset. // 添加 `MUL` 指令來將堆疊中的 RHS 值乘以暫存器中的整數大小, 以獲得位址偏移量
      if ((ty = t) > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;  }

      // Add addition instruction to add LHS value on stack to RHS value in
      // register. // 添加加法指令來將堆疊中的 LHS 值加上暫存器中的 RHS 值
      *++e = ADD;
    }
    // If current token is subtraction operator. // 如果當前 token 是減法運算符
    else if (tk == Sub) {
      // Read token. // 讀取 token
      // Add `PSH` instruction to push LHS value in register to stack.
      // 添加 `PSH` 指令來將暫存器中的 LHS 值推進堆疊
      // Parse RHS expression. // 解析 RHS 表達式
      next(); *++e = PSH; expr(Mul);
      // If LHS value type is pointer and RHS value type is pointer,
      // 如果 LHS 值型態是指標且 RHS 值型態是指標
      // the subtraction result should be divided by int size to get int-size
      // difference. // 則減法的結果應該除以整數大小以獲得整數大小的差異
      // Add `SUB` instruction to subtract LHS value on stack by RHS value in
      // register. // 添加 `SUB` 指令來將堆疊中的 `LHS` 值減去暫存器中的 `RHS` 值
      // Add `PSH` instruction to push the address difference in register to
      // stack. // 添加 `PSH` 指令來將暫存器中的位址差異推進堆疊
      // Add `IMM` instruction to load int size to register.
      // 添加 `IMM` 指令來將整數大小載入到暫存器
      // Add `DIV` instruction to divide the address difference on stack by the
      // int size in register to get int-size difference.
      // 添加 `DIV` 指令來將堆疊中的位址差異除以暫存器中的整數大小, 以獲得整數大小的差異
      if (t > PTR && t == ty) { *++e = SUB; *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = DIV; ty = INT; }
      // If LHS value type is pointer and RHS value type is not pointer,
      // 如果 LHS 值型態不是指標且 RHS 值型態不是指標
      // the RHS value should be multiplied by int size to get address offset.
      // 則 RHS 值應該乘以整數大小以獲得位址偏移量
      // Add `PSH` instruction to push LHS value in register to stack.
      // 添加 `PSH` 指令來將暫存器中的 LHS 值推進堆疊
      // Add `IMM` instruction to load int size to register.
      // 添加 `IMM` 指令來將整數大小載入到暫存器
      // Add `MUL` instruction to multiply RHS value on stack by int size in
      // register to get the address offset. // 添加 `MUL` 指令來將堆疊中的 RHS 值乘以暫存器中的整數大小, 以獲得位址偏移量
      // Add 'SUB' instruction to subtract LHS value on stack by the address
      // offset in register. // 添加 `SUB` 指令來將堆疊中的 LHS 值減去暫存器中的位址偏移量
      else if ((ty = t) > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL; *++e = SUB; }
      // If LHS value type is not pointer. // 如果 LHS 值型態不是指標
      // Add `SUB` instruction to subtract LHS value on stack by RHS value in
      // register. // 添加 `SUB` 指令來將堆疊中的 `LHS` 值減去暫存器中的 `RHS` 值
      else *++e = SUB;
    }
    // If current token is multiplication operator. // 如果當前 token 是乘法運算符
    // Add `PSH` instruction to push LHS value in register to stack.
    // 添加 `PSH` 指令來將暫存器中的 LHS 值推進堆疊
    // Parse RHS expression. // 解析 RHS 表達式
    // Add `MUL` instruction to multiply LHS value on stack by RHS value in
    // register. // 添加 `MUL` 指令來將堆疊中的 `LHS` 值乘以暫存器中的 `RHS` 值
    // Set result value type be `INT`. // 將結果值型態設為整數
    // The following operators are similar. // 下面的運算符也類似這樣
    else if (tk == Mul) { next(); *++e = PSH; expr(Inc); *++e = MUL; ty = INT; }
    else if (tk == Div) { next(); *++e = PSH; expr(Inc); *++e = DIV; ty = INT; }
    else if (tk == Mod) { next(); *++e = PSH; expr(Inc); *++e = MOD; ty = INT; }
    // If current token is postfix increment or decrement operator.
    // 如果當前 token 是後置遞增或遞減運算符
    else if (tk == Inc || tk == Dec) {
      // If current instruction is `LC`, insert a `PSH` instruction before `LC`
      // 如果當前指令是 `LC`, 在 `LC` 之前插入一個 `PSH` 指令
      // to push variable address in register to stack for use by the `SC`
      // 來將暫存器中的變數位址推進堆疊, 供下面添加的 `SC` 指令使用
      // instruction added below.
      if (*e == LC) { *e = PSH; *++e = LC; }
      // If current instruction is `LI`, insert a `PSH` instruction before `LI`
      // 如果當前指令是 `LI`, 在 `LI` 之前插入一個 `PSH` 指令
      // to push variable address in register to stack for use by the `SI`
      // 來將暫存器中的變數位址推進堆疊, 供下面添加的 `SI` 指令使用
      // instruction added below.
      else if (*e == LI) { *e = PSH; *++e = LI; }
      // Else print error and exit program. // 否則印出錯誤並退出程序
      else { printf("%d: bad lvalue in post-increment\n", line); exit(-1); }

      // Add `PSH` instruction to push operand value in register to stack.
      // 添加 `PSH` 指令來將暫存器中的運算元推進堆疊
      // Add `IMM` instruction to load increment/decrement size to register.
      // 添加 `IMM` 指令來將遞增/遞減大小載入到暫存器中
      *++e = PSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);

      // Add `ADD`/`SUB` instruction to compute the post value.
      // 添加 `ADD`/`SUB` 指令來計算後置值
      *++e = (tk == Inc) ? ADD : SUB;

      // Add `SC`/`SI` instruction to save the post value in register to
      // variable. // 添加 `SC`/`SI` 指令來將暫存器中的後置值保存到變數中
      *++e = (ty == CHAR) ? SC : SI;

      // Add `PSH` instruction to push the post value in register to stack.
      // 添加 `PSH` 指令來將暫存器中的後置值推進堆疊
      // Add `IMM` instruction to load increment/decrement size to register.
      // 添加 `IMM` 指令來將遞增/遞減大小載入到暫存器中
      *++e = PSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);

      // Add `SUB`/`ADD` instruction to compute the old value.
      // 添加 `SUB`/`ADD` 指令來計算舊值
      // This implements postfix semantics.
      // 這實現了後置語義
      *++e = (tk == Inc) ? SUB : ADD;

      // Read token. // 讀取 token
      next();
    }
    // If current token is `[`, it is array subscript. // 如果當前 token 是左方括號, 則他是陣列索引
    else if (tk == Brak) {
      // Read token. // 讀取 token
      // Add `PSH` instruction to push the base address in register to stack.
      // 添加 `PSH` 指令來將暫存器中的基底位址推進堆疊
      // Parse subscript expression. // 解析索引表達式
      next(); *++e = PSH; expr(Assign);

      // If current token is not `]`, print error and exit program. // 如果當前 token 不是右方括號, 印出錯誤並退出程序
      if (tk == ']') next(); else { printf("%d: close bracket expected\n", line); exit(-1); }

      // If base address's value type is int pointer or pointer to pointer,
      // 如果基底位址的值型態是整數指標或雙重指標
      // the subscript value should be multiplied by int size to get address
      // 則索引值應該乘以整數大小以獲得位址偏移量
      // offset. `t == PTR` is char pointer `char*`, which needs not doing so.
      // `t == PTR` 是字元指標 `char*`, 不需要進行這樣的操作
      // Add `PSH` instruction to push subscript value in register to stack.
      // 添加 `PSH` 指令來將暫存器中的索引值推進堆疊
      // Add `IMM` instruction to load int size to register.
      // 添加 `IMM` 指令來將整數大小載入到暫存器
      // Add `MUL` instruction to compute address offset.
      // 添加 `MUL` 指令來計算位址偏移量
      if (t > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;  }
      // If base address's value type is not pointer, print error and exit
      // program. // 如果基底位址的值型態不是指標, 印出錯誤並退出程序
      else if (t < PTR) { printf("%d: pointer type expected\n", line); exit(-1); }

      // Add `ADD` instruction to add the address offset to the base address.
      // 添加 `ADD` 指令來將位址偏移量加到基底位址上
      *++e = ADD;

      // Add `LC`/`LI` instruction to load the value on the address in register
      // to register. // 添加 `LC`/`LI` 指令來將暫存器中的位址所指向的值載入到暫存器
      *++e = ((ty = t - PTR) == CHAR) ? LC : LI;
    }
    // If current token is not a known operator, print error and exit program.
    // 如果當前 token 不是已知運算符, 印出錯誤並退出程序
    else { printf("%d: compiler error tk=%d\n", line, tk); exit(-1); }
  }
}

// Parse statement. // 解析語句
void stmt()
{
  int *a, *b;

  // If current token is `if`. // 如果當前 token 是 if
  if (tk == If) {
    // Read token. // 讀取 token
    next();

    // If current token is not `(`, print error and exit program. // 如果當前 token 不是 '(', 則印出錯誤並退出程式
    if (tk == '(') next(); else { printf("%d: open paren expected\n", line); exit(-1); }

    // Parse test expression. // 解析條件運算式
    expr(Assign);

    // If current token is not `)`, print error and exit program. // 如果當前 token 不是 ')', 則印出錯誤並退出程式
    if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }

    // Add jump-if-zero instruction `BZ` to jump over the true branch. // 添加條件跳轉指令 BZ 以跳過 true 區塊
    // Point `b` to the jump address field to be patched later. // 將 b 指向待修補的跳轉位址欄位
    *++e = BZ; b = ++e;

    // Parse true branch's statement. // 解析 true 區塊的語句
    stmt();

    // If current token is `else`. // 如果當前 token 是 else
    if (tk == Else) {
      // Patch the jump address field pointed to by `b` to hold the address of else branch. // 將 b 所指向的跳轉位址欄位修補為 else 區塊的起始位址
      // `e + 3` excludes the `JMP` instruction added below. // e + 3 是因為還會再插入一個 JMP
      // Add `JMP` instruction after the true branch to jump over the else branch. // 在 true 區塊後插入 JMP 以跳過 else 區塊
      // Point `b` to the jump address field to be patched later. // b 指向新的跳轉位址欄位 (for else 區塊結束)
      *b = (int)(e + 3); *++e = JMP; b = ++e;

      // Read token. // 讀取 token
      next();

      // Parse else branch's statement. // 解析 else 區塊的語句
      stmt();
    }

    // Patch the jump address field pointed to by `b` to hold the address past the if-else structure. // 修補 b 所指向的跳轉位址為 if-else 區塊後的下一行指令
    *b = (int)(e + 1);
  }
  // If current token is `while`. // 如果當前 token 是 while
  else if (tk == While) {
    // Read token. // 讀取 token
    next();

    // Point `a` to the loop's test expression's address. // a 指向 while 條件表達式的位置
    a = e + 1;

    // If current token is not `(`, print error and exit program. // 如果 token 不是 '(', 印出錯誤並退出
    if (tk == '(') next(); else { printf("%d: open paren expected\n", line); exit(-1); }

    // Parse test expression. // 解析條件運算式
    expr(Assign);

    // If current token is not `)`, print error and exit program. // 如果 token 不是 ')', 印出錯誤並退出
    if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }

    // Add jump-if-zero instruction `BZ` to jump over loop body. // 插入條件跳轉指令 BZ 來跳過 while 迴圈的主體
    // Point `b` to the jump address field to be patched later. // b 為待修補的跳轉地址
    *++e = BZ; b = ++e;

    // Parse loop body's statement. // 解析 while 主體
    stmt();

    // Add `JMP` instruction to jump to test expression. // 插入 JMP 回到 while 條件開始處
    *++e = JMP; *++e = (int)a;

    // Patch the jump address field pointed to by `b` to hold the address past the loop structure. // 修補 b 所指向的跳轉地址為 while 區塊之後的地址
    *b = (int)(e + 1);
  }
  // If current token is `return`. // 如果當前 token 是 return
  else if (tk == Return) {
    // Read token. // 讀取 token
    next();

    // If current token is not `;`, it is return expression. // 如果不是分號，表示有傳回值
    // Parse return expression. // 解析回傳值運算式
    if (tk != ';') expr(Assign);

    // Add `LEV` instruction to leave the function. // 插入 LEV 指令表示離開函式 (return)
    *++e = LEV;

    // If current token is `;`, read token, else print error and exit program. // 確認後面是分號，否則印出錯誤
    if (tk == ';') next(); else { printf("%d: semicolon expected\n", line); exit(-1); }
  }
  // If current token is `{`, it is block. // 如果當前 token 是 '{', 表示語句區塊 (複合語句)
  else if (tk == '{') {
    // Read token. // 讀取 token
    next();

    // While current token is not `}`. // 直到遇到 '}' 為止
    // Parse statement. // 逐個解析語句
    while (tk != '}') stmt();

    // Read token. // 跳過 '}'
    next();
  }
  // If current token is `;`, it is statement end. // 如果當前 token 是 ';'，代表空語句 (do nothing)
  else if (tk == ';') {
    // Read token. // 讀取 token
    next();
  }
  // If current token is none of above, assume it is expression. // 否則視為運算式語句 (表達式 + 分號)
  else {
    // Parse expression. // 解析運算式
    expr(Assign);

    // If current token is `;`, read token, else print error and exit program. // 後面必須接分號，否則印出錯誤
    if (tk == ';') next(); else { printf("%d: semicolon expected\n", line); exit(-1); }
  }
}

int main(int argc, char **argv)
{
  int fd, bt, ty, poolsz, *idmain;
  int *pc, *sp, *bp, a, cycle; // vm registers // 虛擬機暫存器
  int i, *t; // temps // 暫存變數

  // Decrement `argc` to get the number of command line arguments. // 遞減 `argc` 取得參數數量
  // Increment `argv` to point to the first command line argument. // 遞增 `argv` 指向第一個參數
  --argc; ++argv;

  // If command line argument `-s` is given, // 若指定 `-s` 參數
  // turn on switch for printing source code line and corresponding // 開啟印出原始碼與對應指令的開關
  // instructions. // 印出對應的虛擬機指令
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }

  // If command line argument `-d` is given, // 若指定 `-d` 參數
  // turn on debug switch. // 開啟除錯模式
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }

  // If source code file path is not given, print program usage and exit // 若沒指定原始碼檔案，印出使用方式後結束
  // program. // 結束程式
  if (argc < 1) { printf("usage: c4 [-s] [-d] file ...\n"); return -1; }

  // Open source code file. // 開啟原始碼檔案
  // If failed, print error and exit program. // 若失敗則印出錯誤訊息並結束
  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

  // Set buffer size. // 設定緩衝區大小
  poolsz = 256*1024; // arbitrary size // 任意指定的大小

  // Allocate symbol table. // 配置符號表
  // If failed, print error and exit program. // 若失敗則印出錯誤訊息並結束
  if (!(sym = malloc(poolsz))) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; }

  // Allocate instruction buffer. // 配置指令區
  // If failed, print error and exit program. // 若失敗則印出錯誤訊息並結束
  if (!(le = e = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; }

  // Allocate data buffer. // 配置資料區
  // If failed, print error and exit program. // 若失敗則印出錯誤訊息並結束
  if (!(data = malloc(poolsz))) { printf("could not malloc(%d) data area\n", poolsz); return -1; }

  // Allocate stack. // 配置堆疊區
  // If failed, print error and exit program. // 若失敗則印出錯誤訊息並結束
  if (!(sp = malloc(poolsz))) { printf("could not malloc(%d) stack area\n", poolsz); return -1; }

  // Clear the buffers. // 清空所有緩衝區
  memset(sym,  0, poolsz);
  memset(e,    0, poolsz);
  memset(data, 0, poolsz);

  // Keywords and system call names. // 關鍵字與系統呼叫名稱
  p = "char else enum if int return sizeof while "
      "open read close printf malloc free memset memcmp exit void main";

  // For each keyword from `char` to `while`, // 對每個關鍵字從 `char` 到 `while`
  // call `next` to create symbol table entry, // 呼叫 `next` 建立符號表項目
  // store the keyword's token type in the symbol table entry's `Tk` field. // 將 token 類型存入 `Tk` 欄位
  i = Char; while (i <= While) { next(); id[Tk] = i++; } // add keywords to symbol table // 加入關鍵字至符號表

  // For each system call name from `open` to `exit`, // 對每個系統呼叫從 `open` 到 `exit`
  // call `next` to create symbol table entry, // 呼叫 `next` 建立符號表項目
  // set the symbol table entry's symbol type field be `Sys`, // 將類型設為 `Sys`
  // set the symbol table entry's associated value type field be the system // 將回傳型別設為整數
  // call's return type, // 系統呼叫的回傳型態
  // set the symbol table entry's associated value field be the system call's // 將系統呼叫的編號存入 `Val`
  // opcode. // 系統呼叫的操作碼
  i = OPEN; while (i <= EXIT) { next(); id[Class] = Sys; id[Type] = INT; id[Val] = i++; } // add library to symbol table // 加入標準函式庫至符號表

  // Create symbol table entry for `void`. // 建立 `void` 的符號表項目
  next(); id[Tk] = Char; // handle void type // 處理 void 類型

  // Create symbol table entry for `main`. // 建立 `main` 的符號表項目
  // Point `idmain` to the symbol table entry. // 將 `idmain` 指向該項目
  next(); idmain = id; // keep track of main // 記錄 main 的位置

  // Allocate source code buffer. // 配置原始碼區域
  // If failed, print error and exit program. // 若失敗則印出錯誤訊息並結束
  if (!(lp = p = malloc(poolsz))) { printf("could not malloc(%d) source area\n", poolsz); return -1; }

  // Read source code from source code file into source code buffer. // 讀入原始碼至緩衝區
  // If failed, print error and exit program. // 若失敗則印出錯誤訊息並結束
  if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %d\n", i); return -1; }

  // Add end maker `\0` after the source code in source code buffer. // 在最後補上字串結尾符號 '\0'
  p[i] = 0;

  // Close source code file. // 關閉原始碼檔案
  close(fd);

  // parse declarations // 解析宣告
  line = 1;

  // Read token. // 讀取 token
  next();

  // While current token is not input end. // 當 token 不是檔案結尾
  while (tk) {
    // Set result value type. // 設定基本型別
    bt = INT; // basetype // 預設為 int

    // If current token is `int`, read token. // 若為 int 則讀取下一個 token
    if (tk == Int) next();
    // If current token is `char`, read token, set result value type be `CHAR`. // 若為 char 則設為 CHAR
    else if (tk == Char) { next(); bt = CHAR; }
    // If current token is `enum`, it is enum definition. // 若為 enum 則處理列舉定義
    else if (tk == Enum) {
      // Read token. // 讀取下一個 token
      next();

      // If current token is not `{`, it means having enum type name. // 若非 `{` 表示 enum 有名字
      // Skip the enum type name. // 跳過 enum 名稱
      if (tk != '{') next();

      // If current token is `{`. // 如果是 `{`
      if (tk == '{') {
        // Read token. // 讀取下一個 token
        next();

        // Enum value starts from 0. // 列舉值從 0 開始
        i = 0;

        // While current token is not `}` // 當不是 `}`
        while (tk != '}') {
          // Current token should be enum name. // token 應該是列舉名稱
          // If current token is not identifier, print error and exit program. // 若不是名稱則錯誤
          if (tk != Id) { printf("%d: bad enum identifier %d\n", line, tk); return -1; }

          // Read token. // 讀取下一個 token
          next();

          // If current token is assignment operator. // 若有指定數值
          if (tk == Assign) {
            // Read token. // 讀取下一個 token
            next();

            // If current token is not number constant, print error and exit // 若非數字常數則錯誤
            // program.
            if (tk != Num) { printf("%d: bad enum initializer\n", line); return -1; }

            // Set enum value. // 設定 enum 值
            i = ival;

            // Read token. // 讀取下一個 token
            next();
          }

          // `id` is pointing to the enum name's symbol table entry. // `id` 指向 enum 名稱的符號表
          // Set the symbol table entry's symbol type be `Num`. // 類型設為 Num
          // Set the symbol table entry's associated value type be `INT`. // 設為整數型別
          // Set the symbol table entry's associated value be the enum value. // 設為對應的數值
          id[Class] = Num; id[Type] = INT; id[Val] = i++;

          // If current token is `,`, skip. // 若為逗號，跳過
          if (tk == ',') next();
        }

        // Skip `}`. // 跳過 `}`
        next();
      }
    }

    // While current token is not statement end or block end. // 當前 token 不是語句結尾或區塊結尾時
    while (tk != ';' && tk != '}') {
      // Set value type. // 設定變數型態
      ty = bt;

      // While current token is `*`, it is pointer type. // 當前 token 是 `*` 表示指標型態
      // Read token. // 讀取下一個 token
      // Add `PTR` to the value type. // 將 PTR 加到變數型態上
      while (tk == Mul) { next(); ty = ty + PTR; }

      // Current token should be variable name or function name. // 接下來的 token 應該是變數或函數名稱
      // If current token is not identifier, print error and exit program. // 如果不是識別子，印出錯誤並結束
      if (tk != Id) { printf("%d: bad global declaration\n", line); return -1; }

      // If the name has been defined before, print error and exit program. // 如果這個名稱已經定義過，印出錯誤並結束
      if (id[Class]) { printf("%d: duplicate global definition\n", line); return -1; }

      // Read token. // 讀取下一個 token
      next();

      // Store the variable's data type or the function's return type. // 儲存變數或函數的資料型態
      id[Type] = ty;

      // If current token is `(`, it is function definition. // 如果下一個 token 是 `(`，則是函數定義
      if (tk == '(') { // function // 函數
        // Store symbol type. // 設定符號類型為函數
        id[Class] = Fun;

        // Store function address. // 儲存函數地址
        // `+ 1` is because the code to add instruction always uses `++e`. // `+1` 是因為指令加入時使用 `++e`
        id[Val] = (int)(e + 1);

        // Read token. // 讀取下一個 token
        // `i` is parameter's index. // `i` 是參數索引
        next(); i = 0;

        // Parse parameters list. // 解析參數列表
        // While current token is not `)`. // 當前 token 不是 `)` 時
        while (tk != ')') {
          // Set current parameter's data type. // 預設參數型態為 INT
          ty = INT;

          // If current parameter's data type is `int`, read token. // 如果是 int，讀取下一個 token
          if (tk == Int) next();
          // If current parameter's data type is `char`, read token, set // 如果是 char，讀取並設定為 CHAR
          // data type be `CHAR`. // 設定參數型態為 CHAR
          else if (tk == Char) { next(); ty = CHAR; }

          // While current token is `*`, it is pointer type. // 如果是 `*` 表示指標型態
          // Add `PTR` to the data type. // 加上 PTR
          while (tk == Mul) { next(); ty = ty + PTR; }

          // Current token should be parameter name. // 接下來應為參數名稱
          // If current token is not identifier, print error and exit program. // 否則印出錯誤
          if (tk != Id) { printf("%d: bad parameter declaration\n", line); return -1; }

          // If the parameter name has been defined before as parameter, print // 如果該名稱已定義為參數，印出錯誤
          // error and exit program. // 結束程式
          if (id[Class] == Loc) { printf("%d: duplicate parameter definition\n", line); return -1; }

          // Back up the symbol's `Class`, `Type`, `Val` fields because they // 備份符號欄位
          // will be used temporarily for the parameter name. // 這些欄位會暫時使用
          // Set the symbol type be local variable. // 設為區域變數
          // Set the associated value type be the parameter's data type. // 設定參數型態
          // Store the parameter's index. // 儲存參數索引
          id[HClass] = id[Class]; id[Class] = Loc;
          id[HType]  = id[Type];  id[Type] = ty;
          id[HVal]   = id[Val];   id[Val] = i++;

          // Read token. // 讀取下一個 token
          next();

          // If current token is `,`, skip. // 如果是逗號，繼續處理下一個參數
          if (tk == ',') next();
        }

        // Read token. // 讀取 `)`
        next();

        // If current token is not function body's `{`, print error and exit // 如果不是 `{`，印出錯誤
        // program. // 結束程式
        if (tk != '{') { printf("%d: bad function definition\n", line); return -1; }

        // Local variable offset. // 區域變數偏移量初始設定
        loc = ++i;

        // Read token. // 讀取 `{`
        next();

        // While current token is `int` or `char`, it is variable definition. // 如果是 int 或 char，表示變數定義
        while (tk == Int || tk == Char) {
          // Set base data type. // 設定基本型態
          bt = (tk == Int) ? INT : CHAR;

          // Read token. // 讀取下一個 token
          next();

          // While statement end is not met. // 直到遇到 `;` 為止
          while (tk != ';') {
            // Set base data type. // 設定變數型態
            ty = bt;

            // While current token is `*`, it is pointer type. // 如果是 `*` 表示指標型態
            // Add `PTR` to the data type. // 加上 PTR
            while (tk == Mul) { next(); ty = ty + PTR; }

            // Current token should be local variable name. // 接下來應該是變數名稱
            // If current token is not identifier, print error and exit // 如果不是識別子，印出錯誤
            // program. // 結束程式
            if (tk != Id) { printf("%d: bad local declaration\n", line); return -1; }

            // If the local variable name has been defined before as local // 如果此名稱已定義過，印出錯誤
            // variable, print error and exit program. // 結束程式
            if (id[Class] == Loc) { printf("%d: duplicate local definition\n", line); return -1; }

            // Back up the symbol's `Class`, `Type`, `Val` fields because they // 備份原本的欄位值
            // will be used temporarily for the local variable name. // 這些欄位會被暫時覆寫
            // Set the symbol type be local variable. // 設為區域變數
            // Set the associated value type be the local variable's data type. // 設定型態
            // Store the local variable's index. // 儲存變數偏移位置
            id[HClass] = id[Class]; id[Class] = Loc;
            id[HType]  = id[Type];  id[Type] = ty;
            id[HVal]   = id[Val];   id[Val] = ++i;

            // Read token. // 讀取下一個 token
            next();

            // If current token is `,`, skip. // 如果是逗號，繼續處理下一個變數
            if (tk == ',') next();
          }

          // Read token. // 讀取 `;`
          next();
        }

        // Add `ENT` instruction before function body. // 在函數主體前插入 `ENT` 指令
        // Add local variables count as operand. // 並插入區域變數數量
        *++e = ENT; *++e = i - loc;

        // While current token is not function body's ending `}`, // 當不是 `}` 時
        // parse statement. // 解析語句
        while (tk != '}') stmt();

        // Add `LEV` instruction after function body. // 在函數結尾插入 `LEV` 指令
        *++e = LEV;

        // Point `id` to symbol table. // 回到符號表的起始
        id = sym; // unwind symbol table locals // 還原符號表

        // While current symbol table entry is in use. // 當前條目仍在使用中
        while (id[Tk]) {
          // If the symbol table entry is for function parameter or local
          // variable. // 如果是參數或區域變數
          if (id[Class] == Loc) {
            // Restore `Class`, `Type` and `Val` fields' old value. // 還原原本的欄位值
            id[Class] = id[HClass];
            id[Type] = id[HType];
            id[Val] = id[HVal];
          }

          // Point to next symbol table entry. // 移動到下一個條目
          id = id + Idsz;
        }
      }
      // If current token is not `(`, then it is not function definition, // 如果不是函數定義
      // assume it is global variable definition. // 則是全域變數
      else {
        // Set symbol type. // 設定為全域變數
        id[Class] = Glo;

        // Store the global variable's address. // 儲存變數地址
        id[Val] = (int)data;

        // Point to next global variable. // 指向下一個變數位置
        data = data + sizeof(int);
      }

      // If current token is `,`, skip. // 如果是逗號，處理下一個變數
      if (tk == ',') next();
    }

    // Read token. // 最後讀取分號或右大括號
    next();
  }

  // Point instruction pointer `pc` to `main` function's address. // 將指令指標 `pc` 指向 `main` 函數的位址
  // If symbol `main`'s `Val` field is not set, it means `main` function is
  // not defined, print error and exit program. // 如果 `main` 函數的 `Val` 欄位尚未設置，表示沒有定義 `main` 函數，印出錯誤訊息並退出
  if (!(pc = (int *)idmain[Val])) { printf("main() not defined\n"); return -1; }

  // If switch for printing source code line and corresponding instructions is
  // on, exit program. // 如果有開啟印出原始碼及對應指令的選項，則退出程式
  if (src) return 0;

  // setup stack // 設定堆疊
  // Point frame base pointer `bp` and stack top pointer `sp` to stack bottom. // 將基底指標 `bp` 和堆疊頂端 `sp` 指向堆疊底部
  bp = sp = (int *)((int)sp + poolsz);

  // Push `EXIT` instruction to stack. // 將 `EXIT` 指令壓入堆疊
  // Note the stack grows towards lower address so after the `PSH` instruction
  // added below is executed, this `EXIT` instruction will be executed to exit
  // the program. // 注意堆疊是向低位址成長，所以當 `PSH` 指令執行完後，這個 `EXIT` 會被執行以結束程式
  *--sp = EXIT; // call exit if main returns // 如果 main 返回則執行 exit

  // Push `PSH` instruction to stack to push exit code in register to stack
  // after `main` function returns. The exit code on stack will be used by the
  // `EXIT` instruction added above. // 將 `PSH` 指令壓入堆疊，在 `main` 返回後推入退出碼，此值會被上面的 `EXIT` 取用
  // Point `t` to the `PSH` instruction's address. // `t` 指向 `PSH` 指令的位址
  *--sp = PSH; t = sp;

  // Push `main` function's first argument `argc` to stack. // 將 `main` 的第一個參數 `argc` 壓入堆疊
  *--sp = argc;

  // Push `main` function's second argument `argv` to stack. // 將 `main` 的第二個參數 `argv` 壓入堆疊
  *--sp = (int)argv;

  // Push the `PSH` instruction's address to stack so that `main` function
  // will return to the `PSH` instruction. // 將 `PSH` 的位址壓入堆疊，使 `main` 函數能返回此處繼續執行
  *--sp = (int)t;

  // run... // 開始執行虛擬機器指令
  // Instruction cycles count. // 指令執行的週期計數
  cycle = 0;
  // Run VM loop to execute VM instructions. // 執行虛擬機器迴圈以執行指令
  while (1) {
    // Get current instruction. // 取得當前指令
    // Increment instruction pointer. // 將指令指標往後移動
    // Increment instruction cycles count. // 增加執行週期計數
    i = *pc++; ++cycle;

    // If debug switch is on. // 如果開啟除錯模式
    if (debug) {
      // Print opcode. // 印出操作碼
      printf("%d> %.4s", cycle,
        &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
         "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
         "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[i * 5]);

      // If the opcode <= ADJ, it has operand. // 如果操作碼小於等於 ADJ，表示它有運算元
      // Print operand. // 印出運算元
      if (i <= ADJ) printf(" %d\n", *pc); else printf("\n");
    }

    // Add the base address in frame base pointer `bp` to the offset in the
    // operand. // 將基底指標 `bp` 加上運算元中的偏移量
    if      (i == LEA) a = (int)(bp + *pc++);                             // load local address // 載入區域變數位址
    // Load the operand to register. // 將運算元載入到暫存器
    else if (i == IMM) a = *pc++;                                         // load global address or immediate // 載入全域變數位址或立即值
    // Jump to the address in the operand. // 跳到運算元中的位址
    else if (i == JMP) pc = (int *)*pc;                                   // jump // 無條件跳躍
    // Push the return address in the second operand to stack. // 將返回位址推入堆疊
    // Jump to the address in the first operand. // 跳到第一個運算元位址
    else if (i == JSR) { *--sp = (int)(pc + 1); pc = (int *)*pc; }        // jump to subroutine // 呼叫子程式
    // Jump to the address in the first operand if register value is 0. // 如果暫存器值為 0 則跳躍
    else if (i == BZ)  pc = a ? pc + 1 : (int *)*pc;                      // branch if zero // 為零則跳
    // Jump to the address in the first operand if register value is not 0. // 如果暫存器值不為 0 則跳躍
    else if (i == BNZ) pc = a ? (int *)*pc : pc + 1;                      // branch if not zero // 不為零則跳
    // Push the caller's frame base address in `bp` to stack. // 將目前函數的基底指標 `bp` 推入堆疊
    // Point `bp` to stack top for the callee. // 設定新函數的基底指標為目前堆疊頂
    // Decrease stack top pointer `sp` by the value in the operand to reserve // 將堆疊頂 `sp` 減去運算元值以保留區域變數空間
    // space for the callee's local variables. // 為子函數的區域變數保留空間
    else if (i == ENT) { *--sp = (int)bp; bp = sp; sp = sp - *pc++; }     // enter subroutine // 進入子程式
    // Pop arguments off stack after returning from function call. // 函數呼叫返回後從堆疊中移除參數
    else if (i == ADJ) sp = sp + *pc++;                                   // stack adjust // 調整堆疊
    // Point stack top pointer `sp` to caller's stack top before the call. // 將堆疊頂回復為呼叫者狀態
    // Pop caller's frame base address off stack into `bp`. // 從堆疊取出呼叫者的 `bp`
    // The old value was pushed to stack by `ENT` instruction. // 這是由 `ENT` 指令儲存的
    else if (i == LEV) { sp = bp; bp = (int *)*sp++; pc = (int *)*sp++; } // leave subroutine // 離開子程式
    // Load int value on the address in register to register. // 將暫存器中位址對應的 int 值載入
    else if (i == LI)  a = *(int *)a;                                     // load int // 載入整數
    // Load char value on the address in register to register. // 將暫存器中位址對應的 char 值載入
    else if (i == LC)  a = *(char *)a;                                    // load char // 載入字元
    // Save int value in register to address on stack. // 將暫存器中的整數存入堆疊位址
    else if (i == SI)  *(int *)*sp++ = a;                                 // store int // 儲存整數
    // Save char value in register to address on stack. // 將暫存器中的字元存入堆疊位址
    else if (i == SC)  a = *(char *)*sp++ = a;                            // store char // 儲存字元
    // Push register value to stack. // 將暫存器值推入堆疊
    else if (i == PSH) *--sp = a;                                         // push // 推入堆疊

    // The following instructions take two arguments. // 以下指令需要兩個參數
    // The first argument is on stack. // 第一個參數在堆疊中
    // The second argument is in register. // 第二個參數在暫存器中
    // The result is put to register. // 結果寫入暫存器
    else if (i == OR)  a = *sp++ |  a; // OR // 位元或
    else if (i == XOR) a = *sp++ ^  a; // XOR // 位元異或
    else if (i == AND) a = *sp++ &  a; // AND // 位元與
    else if (i == EQ)  a = *sp++ == a; // EQ // 等於
    else if (i == NE)  a = *sp++ != a; // NE // 不等於
    else if (i == LT)  a = *sp++ <  a; // LT // 小於
    else if (i == GT)  a = *sp++ >  a; // GT // 大於
    else if (i == LE)  a = *sp++ <= a; // LE // 小於等於
    else if (i == GE)  a = *sp++ >= a; // GE // 大於等於
    else if (i == SHL) a = *sp++ << a; // SHL // 左移
    else if (i == SHR) a = *sp++ >> a; // SHR // 右移
    else if (i == ADD) a = *sp++ +  a; // ADD // 加法
    else if (i == SUB) a = *sp++ -  a; // SUB // 減法
    else if (i == MUL) a = *sp++ *  a; // MUL // 乘法
    else if (i == DIV) a = *sp++ /  a; // DIV // 除法
    else if (i == MOD) a = *sp++ %  a; // MOD // 取餘數

    // The following instructions are system calls. // 以下是系統呼叫指令
    // They take arguments from stack, just like a user-defined function does. // 他們從堆疊中取得參數，就像使用者自定函數一樣
    // Note the stack grows towards lower address, arguments pushed earlier are // 注意堆疊向下成長，先推入的參數在高位址
    // at higher address. E.g. if there are three arguments on stack, then: // 例如有三個參數時：
    // `sp[2]` is the first argument. // 第一個參數是 sp[2]
    // `sp[1]` is the second argument. // 第二個參數是 sp[1]
    // `*sp` is the third argument. // 第三個參數是 *sp
    //
    // Open file. // 開啟檔案
    // Arg 1: The file path to open. // 參數 1：檔案路徑
    // Arg 2: The flags. // 參數 2：開啟模式
    else if (i == OPEN) a = open((char *)sp[1], *sp);
    // Read from file descriptor into buffer. // 從檔案描述符讀取資料到緩衝區
    // Arg 1: The file descriptor. // 參數 1：檔案描述符
    // Arg 2: The buffer pointer. // 參數 2：緩衝區指標
    // Arg 3: The number of bytes to read. // 參數 3：要讀取的位元組數
    else if (i == READ) a = read(sp[2], (char *)sp[1], *sp);
    // Close file descriptor. // 關閉檔案
    // Arg 1: The file descriptor. // 參數 1：檔案描述符
    else if (i == CLOS) a = close(*sp);
    // Print formatted string. // 印出格式化字串
    // Because the call has arguments, an ADJ instruction should have been
    // added. `pc[1]` gets the ADJ instruction's operand, i.e. the number of
    // arguments. // 因為該呼叫有參數，所以前面應有 ADJ 指令，`pc[1]` 是該 ADJ 的參數數量
    // Arg 1: The format string. // 參數 1：格式字串
    // Arg 2-7: The formatted values. // 參數 2-7：對應格式字串的值
    else if (i == PRTF) { t = sp + pc[1]; a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]); }
    // Allocate memory block. // 配置記憶體區塊
    // Arg 1: The number of bytes to allocate. // 參數 1：要配置的位元組數
    else if (i == MALC) a = (int)malloc(*sp);
    // Free memory block allocated. // 釋放記憶體區塊
    // Arg 1: The memory block pointer. // 參數 1：記憶體區塊的指標
    else if (i == FREE) free((void *)*sp);
    // Set every byte in a memory buffer to the same value. // 將記憶體中的每個位元組設為同一數值
    // Arg 1: The buffer pointer. // 參數 1：緩衝區位址
    // Arg 2: The value. // 參數 2：欲填入的值
    // Arg 3: The number of bytes to set. // 參數 3：欲設定的位元組數
    else if (i == MSET) a = (int)memset((char *)sp[2], sp[1], *sp);
    // Compare memory buffer. // 比較兩段記憶體內容
    // Arg 1: The first buffer pointer. // 參數 1：第一段記憶體
    // Arg 2: The second buffer pointer. // 參數 2：第二段記憶體
    // Arg 3: The number of bytes to compare. // 參數 3：比較的位元組數
    else if (i == MCMP) a = memcmp((char *)sp[2], (char *)sp[1], *sp);
    // Exit program. // 結束程式
    // Arg 1: The exit code. // 參數 1：離開代碼
    else if (i == EXIT) { printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }
    // Current instruction is unknown, print error and exit program. // 不明指令，印出錯誤並結束
    else { printf("unknown instruction = %d! cycle = %d\n", i, cycle); return -1; }
  }
}
