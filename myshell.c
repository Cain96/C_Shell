/*----------------------------------------------------------------------------
 *  簡易版シェル
 *--------------------------------------------------------------------------*/

/*
 *  インクルードファイル
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <dirent.h>

/*
 *  定数の定義
 */

#define BUFLEN    1024     /* コマンド用のバッファの大きさ */
#define MAXARGNUM  256     /* 最大の引数の数 */

/*
 * ローカルプロトタイプ宣言
 */

int parse(char [], char *[]);
void execute_command(char *[], int);

/*----------------------------------------------------------------------------
 *
 *  関数名̾   : main
 *
 *  作業内容 : シェルのプロンプトを実現
 *
 *  引数     :
 *
 *  返り値   :
 *
 *  注意     :
 *
 *--------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    char command_buffer[BUFLEN]; /* コマンド用のバッファ */
    char *args[MAXARGNUM];       /* 引数へのポインタ配列 */
    int command_status;          /* コマンドの状態を表す
                                    command_status = 0 : フォアグラウンドで実行
                                    command_status = 1 : バックグラウンドで実行
                                    command_status = 2 : シェルの終了
                                    command_status = 3 : 何もしない */

    /*
     *  ̵無限ループ
     */

    for(;;) {

        /*
         *  プロンプト表示
         */

        printf("Command : ");

        /*
         *  標準出力から1行を command_buffer へ読み込む
         *  入力が何もなければ回j業を出力してプロンプト表示へ戻る
         */

        if(fgets(command_buffer, BUFLEN, stdin) == NULL) {
            printf("\n");
            continue;
        }
        
       /*
         *  wildcard検討
         */
         wildcard(command_buffer);
         
        /*
         *  入力されたバッファ内のコマンドの解析
         *
         *  返り値はコマンドの状態
         */

        command_status = parse(command_buffer, args);

        /*
         *  終了コマンドならばプログラム終了
         *  引数が何もなければプロンプト表示へ戻る
         */

        if(command_status == 2) {
            printf("done.\n");
            exit(EXIT_SUCCESS);
        } else if(command_status == 3) {
            continue;
        }

        /*
         *  コマンド実行
         */

        execute_command(args, command_status);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 *
 *  関数名̾   : parse
 *
 *  作業内容 : バッファ内のコマンドと引数を解析
 *
 *  引数     :
 *
 *  返り値   : コマンドの状態を表す
                                    0 : フォアグラウンドで実行
                                    1 : バックグラウンドで実行
                                    2 : シェルの終了
                                    3 : 何もしない 
 *
 *  注意     :
 *
 *--------------------------------------------------------------------------*/

int parse(char buffer[],        /* バッファ */
          char *args[])         /* 引数へのポインタ配列 */
{
    int arg_index;   /* 引数用のインデックス */
    int status;   /* コマンドの状態を表す */

    /*
     *  変数の初期化
     */

    arg_index = 0;
    status = 0;

    /*
     *  バッファ内の最後にある改行をヌル文字へ変更
     */

    *(buffer + (strlen(buffer) - 1)) = '\0';

    /*
     *  バッファが終了を表すコマンド ("exit")ならば
     *  コマンドの状態を表す返り値を2に設定してリターン
     */

    if(strcmp(buffer, "exit") == 0) {

        status = 2;
        return status;
    }

    /*
     *  バッファ内の文字がなくなるまで繰り返す
     *  (ヌル文字が出てくるまで繰り返す)
     */

    while(*buffer != '\0') {

        /*
         *  空白類(空白とタブ)をヌル文字に置き換える
         *  これによってバッファ内の各引数が分割される
         */

        while(*buffer == ' ' || *buffer == '\t') {
            *(buffer++) = '\0';
        }

        /*
         * 空白の後の終端文字であればループを抜ける
         */

        if(*buffer == '\0') {
            break;
        }

        /*
         *  空白部分はよみとばされたはず
         *  buffer は現在は arg_index + 1 個目の引数の先頭を指している
         *
         *  引数の先頭へのポインタを引数へのポインタ配列に格納する
         */

        args[arg_index] = buffer;
        ++arg_index;

        /*
         *  引数部分を読み飛ばす
         *  (ヌル文字でも空白類でもない場合に読み進める)
         */

        while((*buffer != '\0') && (*buffer != ' ') && (*buffer != '\t')) {
            ++buffer;
        }
    }

    /*
     *  最後の引数の次にはヌルへのポインタを格納する。
     */

    args[arg_index] = NULL;

    /*
     *  最後の引数をチェックして "&" ならば
     *
     *  "&" を引数から削る
     *  コマンドの状態を表す status に 1 を設定
     *
     *  そうでなければ status に 0 を設定
     */

    if(arg_index > 0 && strcmp(args[arg_index - 1], "&") == 0) {

        --arg_index;
        args[arg_index] = '\0';
        status = 1;

    } else {

        status = 0;

    }

    /*
     *  引数が何もなかった場合
     */

    if(arg_index == 0) {
        status = 3;
    }

    /*
     *  コマンドの状態を返す
     */

    return status;
}

/*----------------------------------------------------------------------------
 *
 *  関数名̾   : excute_command
 *
 *  作業内容 : 引数として与えられたコマンドを実行する
              コマンドの状態がフォアグラウンドならば、コマンドを
              実行している子プロセスの終了を待つ
              バックグラウンドならば子プロセスの終了を待たずに
              main 関数に戻る(プロンプト表示に戻る)
 *
 *  引数     :
 *
 *  返り値   :
 *
 *  注意     :
 *
 *--------------------------------------------------------------------------*/

void execute_command(char *args[],    /* 引数の配列 */
                     int command_status)     /* コマンドの状態 */
{
    int pid;      /* プロセスID */
    int status;   /* 子プロセスの終了ステータス */
    
    /*
     *  内部コマンドの場合
     */

    /*
     *  外部コマンドの場合
     */
    /*
     *  子プロセスの生成
     *
     *  生成できたかを確認し、失敗ならばプログラムを終了
     */

    /******** Your Program ********/
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(-1);
    }

    /*
     *  子プロセスの場合には引数として与えられたものを実行
     *
     *  引数の配列は以下を仮定している
     *  ・第一引数には実行されているプログラムを示す文字列が格納されている
     *  ・引数の配列はヌルポインタで終了している
     */

    /******** Your Program ********/
    if(pid == 0){
        execvp(args[0], args);
        perror(args[0]);
        exit(-1);
    }

    /*
     *  コマンドの状態がバックグラウンドならば、関数を抜ける
     */

    /******** Your Program ********/
    if (command_status == 1){
        return;
    }

    /*
     *  ここに来るのはコマンドの状態がフォアグラウンドの場合
     *  親プロセスの場合に子プロセスの終了を待つ
     */

    /******** Your Program ********/
    wait(&status);

    return;
}

/*-- END OF FILE -----------------------------------------------------------*/
