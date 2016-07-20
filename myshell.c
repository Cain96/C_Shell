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
#define COMMAX     32      /* コマンド履歴の最大 */

/*
 *  構造体の定義
 */
struct node {
    char path[256];
    struct node *next;
};

struct com {
    char command1[256];
    char command2[256];
    struct com *next;
};

/*
 * グローバル変数宣言
 */

int arg_number;

/*
 * ローカルプロトタイプ宣言
 */

int parse(char [], char *[]);
void execute_command(char *[], int, struct node**, struct com**, char [COMMAX][BUFLEN], int, char *);
void cd (char *[]);
void pushd (struct node**);
void dirs (struct node**);
void popd (struct node**);
void wildcard(char *);
void history(char array_history[COMMAX][BUFLEN], int);
void precommand(char *[], struct node ** , struct com **,char [COMMAX][BUFLEN], int, char *);
void alias(char *[], struct com **);
void unalias(char *[], struct com **);
void alias_replace(char *[], struct com **);
void prompt(char *[], char *);

/*----------------------------------------------------------------------------
 *シェルのプロンプトを実現
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
    struct node *head;
    struct com *a_top;
    char array_history[COMMAX][BUFLEN]; /* ヒストリー用配列 */
    int number_cmd = 0;                  /* コマンド数 */
    int i;
    char pmt[256]="Command";    /* プロンプト */
    
    for(i=0; i < COMMAX; i++){
        strcpy(array_history[i], "\0");
    }
    /*
     *  ̵無限ループ
     */
    for(;;) {
       /*
         *  プロンプト表示
         */
        printf("%s : ",pmt);
       /*
         *  標準出力から1行を command_buffer へ読み込む
         *  入力が何もなければ改行を出力してプロンプト表示へ戻る
         */
        if(fgets(command_buffer, BUFLEN, stdin) == NULL) {
            exit(EXIT_SUCCESS);
        } else if(*command_buffer != '\n') {
            if(number_cmd<32){
                strcpy(array_history[number_cmd], command_buffer);
            }else{
                i = number_cmd % COMMAX;
                strcpy(array_history[i], command_buffer);
            }
            number_cmd++;
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
        execute_command(args, command_status, &head, &a_top, array_history, number_cmd, pmt);
    }
    return 0;
}
/*----------------------------------------------------------------------------
 * バッファ内のコマンドと引数を解析
 *  返り値   : コマンドの状態を表す
                                    0 : フォアグラウンドで実行
                                    1 : バックグラウンドで実行
                                    2 : シェルの終了
                                    3 : 何もしない 
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
    
    arg_number = arg_index--;

    /*
     *  コマンドの状態を返す
     */
    return status;
}

/*----------------------------------------------------------------------------
 *  作業内容 : 引数として与えられたコマンドを実行する
              コマンドの状態がフォアグラウンドならば、コマンドを
              実行している子プロセスの終了を待つ
              バックグラウンドならば子プロセスの終了を待たずに
              main 関数に戻る(プロンプト表示に戻る)
 *--------------------------------------------------------------------------*/

void execute_command(char *args[],    /* 引数の配列 */
                     int command_status,     /* コマンドの状態 */
                     struct node **head,      //  スタックの先頭ポインタ
                     struct com **a_top,   //aliasリスト
                     char array_history[COMMAX][BUFLEN],
                     int number_cmd,
                     char *pmt)     /* プロンプト */
{
    int pid;      /* プロセスID */
    int status;   /* 子プロセスの終了ステータス */
    
    alias_replace(args, a_top);//alias機能の検討
    
    /*
     *  内部コマンドの場合
     */
    if(strcmp(args[0], "cd") == 0){
        cd(args);
        return;
    }
    
    if(strcmp(args[0], "pushd") == 0){
        pushd(head);
        return;
    }
    
    if(strcmp(args[0], "dirs") == 0){
        dirs(head);
        return;
    }
    
    if(strcmp(args[0], "popd") == 0){
        popd(head);
        return;
    }
     
     if(strcmp(args[0],"prompt")==0){
        prompt(args, pmt) ;
        return;
    }

    if(strcmp(args[0],"history")==0){
        history(array_history, number_cmd) ;
        return;
    }
    
    if(args[0][0] == '!'){
        precommand(args, head, a_top, array_history, number_cmd, pmt) ;
        return;
    }
    if(strcmp(args[0],"alias")==0){
        alias(args, a_top) ;
        return;
    }
    
    if(strcmp(args[0],"unalias")==0){
        unalias(args, a_top) ;
        return;
    }
     
    /*
     *  外部コマンドの場合
     */
    /*
     *  子プロセスの生成
     *
     *  生成できたかを確認し、失敗ならばプログラムを終了
     */
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
    if(pid == 0){
        execvp(args[0], args);
        perror(args[0]);
        exit(-1);
    }

    /*
     *  コマンドの状態がバックグラウンドならば、関数を抜ける
     */

    if (command_status == 1){
        return;
    }

    /*
     *  ここに来るのはコマンドの状態がフォアグラウンドの場合
     *  親プロセスの場合に子プロセスの終了を待つ
     */

    wait(&status);

    return;
}

/*--------------------------------------------------------------------------*
*  wildcard機能の実装
*--------------------------------------------------------------------------*/

void wildcard (char *command_buffer) {
    char *p;
    char path[256], add[256]="", tmp[256]="";
    DIR *dir;
    struct dirent *dp;
    
    while((p = strstr(command_buffer, "*")) != NULL) {
        getcwd(path,256);
        dir = opendir(path);
        for(dp=readdir(dir);dp!=NULL;dp=readdir(dir)){
            if(dp -> d_type == 8){
                strcat(strcat(add, dp->d_name)," ");
            }
        }
        closedir(dir);
        
        *p = '\0';
        p+=1;
        strcpy(tmp, p);
        strcat(strcat(command_buffer, add),tmp);
    }
    return;
}

/*--------------------------------------------------------------------------*
*  history機能の実装
*--------------------------------------------------------------------------*/
void history (char array_history[COMMAX][BUFLEN], int number_cmd) {
    int i;
    
    if(arg_number == 1){
        if (number_cmd < COMMAX) {
            for(i=0; i < number_cmd; i++){
                printf("[%d] > %s", i, array_history[i]);
            }
        }else {
            int j;
            i = number_cmd - COMMAX;
            j = i;
            for(i; i < COMMAX; i++){
                printf("[%d] > %s", i+1, array_history[i]);
            }
            for(i=0; i < j; i++){
                printf("[%d] > %s", i+COMMAX+1, array_history[i]);
            }
        }
    }else{
        fprintf(stderr,"Irregal Arguments.\n");
    }
    return;
}

/*----------------------------------------------------------------------------
*  alias機能の実装
*--------------------------------------------------------------------------*/
void alias (char *args[], struct com **a_top){
    struct com *now, *prev;
    
    if(arg_number == 1|| arg_number == 3){
        now = *a_top;
        if(args[1] == NULL){ //第二引数なし
            if(now == NULL){
                fprintf(stderr,"Not Found Alias List.\n");
                return;
            }else{
                int i=1;
                while(now != NULL){
                    printf("[%d]:%s => %s\n",i,now->command1,now->command2);
                    now = now->next;
                    i++;
                }
            }
        }else{//第二引数あり
            if(now == NULL){
                now = (struct com *)malloc(sizeof(struct com));  //領域確保
                *a_top = now;
                strcpy(now->command1, args[1]);
                strcpy(now->command2, args[2]);
            }else{
                while(now != NULL){
                    if(strcmp(now->command1, args[1])==0){
                        fprintf(stderr, "It has been already added.\n");
                        return;
                    }
                    prev = now;
                    now = now->next;
                }
                now = (struct com *)malloc(sizeof(struct com));  //領域確保
                strcpy(now->command1, args[1]);
                strcpy(now->command2, args[2]);
                
                prev->next = now;
            }
        }
    }else{
        fprintf(stderr,"Irregal Arguments.\n");
    }
    return;
}

/*----------------------------------------------------------------------------
 *  cd機能の実装
 *--------------------------------------------------------------------------*/
void cd (char *args[]) {
    char current_dir[256];
    char *path;
    
    
    if(arg_number < 3){
        if(args[1] == NULL) {    //引数なしの時
            path = getenv("HOME");
        } else {    //引数ありの時
            if(args[1][0] == '/') {      //絶対パスの時
                path = args[1];
            }else {     //相対パスの時
                getcwd(current_dir, 256);   //カレントディレクトリの取得
                if(strcmp(current_dir,"/")!=0){
                    path = strcat(current_dir, "/");
                    path = strcat(path, args[1]);
                }else{
                    path = strcat(current_dir, args[1]);
                }
            }
        }
        if(path != NULL && chdir(path) == -1){
            fprintf(stderr,"%s is irregal path.\n",path);
        }
    }else{
        fprintf(stderr,"Irregal Arguments.\n");
    }
    return;
}

/*----------------------------------------------------------------------------
 *  prompt機能の実装
 *--------------------------------------------------------------------------*/
void prompt(char *args[],char *pmt){
    if(arg_number < 3){
        if(args[1] == NULL){
            strcpy(pmt, "prompt");
        }else{
            strcpy(pmt, args[1]);
        }
    }else{
        fprintf(stderr,"Irregal Arguments.\n");
    }
    return;
}

/*----------------------------------------------------------------------------
 *  pushd機能の実装
 *--------------------------------------------------------------------------*/
 void pushd(struct node **head){
     struct node *new;
     
     if(arg_number == 1){
        new = (struct node *)malloc(sizeof(struct node));  //領域確保
        new -> next = *head;
     
        if(getcwd(new->path, 256) == NULL){    //エラー時処理
            fprintf(stderr,"pushd Failure\n");
            return;
        }
        *head = new;
    }else{
        fprintf(stderr,"Irregal Arguments.\n");
    }
     return;
 }
 
 /*----------------------------------------------------------------------------
 *  dirs機能の実装
 *--------------------------------------------------------------------------*/
 void dirs(struct node **head) {
     int i=0;
     struct node *new;
     
     if(arg_number == 1){
        if(head == NULL) {
            fprintf(stderr,"There is no in Dirstack");
            return;
        }
     
        new = *head;
     
        while (new != NULL) {
            i++;
            printf("%d : %s\n", i, new->path);
            new = new -> next;
        }
    }else{
        fprintf(stderr,"Irregal Arguments.\n");
    }
     return;
 }
 
/*----------------------------------------------------------------------------
 *  popd機能の実装
 *--------------------------------------------------------------------------*/
 void popd(struct node **head) {
     struct node *post;
     
     if(arg_number == 1){
        post = *head;
     
        if(post==NULL) {
            fprintf(stderr,"There is no in Dirstack\n");
            return;
        }
     
        if(post->path!=NULL && chdir(post->path)==-1){
            fprintf(stderr,"popd Failure\n");
            return;
        }
     
        *head = post -> next;
        free(post);
     
     }else{
        fprintf(stderr,"Irregal Arguments.\n");
    }
     return;
 }
 
/*----------------------------------------------------------------------------
 *  !!/![string]機能の実装
 *--------------------------------------------------------------------------*/
void precommand (char *args[], struct node **head, struct com **a_top,char array_history[COMMAX][BUFLEN], int number_cmd, char *pmt) {
    char cmd[BUFLEN];
    int i;
    int command_status;
    
    if(arg_number == 1){
        if(strcmp(args[0],"!!") == 0) {
            if (number_cmd < COMMAX) {
                if(strcmp(array_history[number_cmd-2],"\0")!=0){
                    strcpy(cmd, array_history[number_cmd-2]);
                    strcpy(array_history[number_cmd-1], cmd);
                } else {
                    fprintf(stderr, "Command Not Found.\n");
                    return;
                }
            } else {
                if((i = (number_cmd-1) % COMMAX) == 0){
                    strcpy(cmd, array_history[32]);
                    strcpy(array_history[number_cmd-1], cmd);
                } else {
                    strcpy(cmd, array_history[i-1]);
                    strcpy(array_history[number_cmd-1], cmd);
                }
            }
        } else {
            if (number_cmd < COMMAX) {
                for(i=number_cmd-2; 0<=i; i--){
                    if(strncmp(array_history[i], args[0]+1, strlen(args[0]+1))==0){
                        strcpy(cmd, array_history[i]);
                        strcpy(array_history[number_cmd-1], cmd);
                        break;
                    }
                    if(i==0){
                        fprintf(stderr,"Command Not Found.\n");
                        return;
                    }
                }
            } else {
                for(i=((number_cmd-2)%COMMAX); 0<=i; i--){
                    if(strncmp(array_history[i], args[0]+1, strlen(args[0]+1))==0){
                        strcpy(cmd, array_history[i]);
                        strcpy(array_history[number_cmd-1], cmd);
                        break;
                    }
                    if(i==0){
                        int j = (number_cmd-1)%COMMAX;
                        int k;
                        for(k=COMMAX; j<k; k--){
                            if(strncmp(array_history[k], args[0]+1, strlen(args[0]+1))==0){
                                strcpy(cmd, array_history[k]);
                                strcpy(array_history[number_cmd-1], cmd);
                                break;
                            }
                            if(k==j+1){
                                fprintf(stderr,"Command Not Found.\n");
                                return;
                            }
                        }
                    }
                }
            }
        }
        /*
            *  入力されたバッファ内のコマンドの解析
            *
            *  返り値はコマンドの状態
            */
        command_status = parse(cmd, args);
            /*
            終了コマンドならばプログラム終了
            *  引数が何もなければプロンプト表示へ戻る
            */
        if(command_status == 2) {
            printf("done.\n");
            exit(EXIT_SUCCESS);
        } else if(command_status == 3) {
            return;
        }
        /*
            *  コマンド実行
            */
        execute_command(args, command_status, head, a_top, array_history, number_cmd, pmt);
     }else{
        fprintf(stderr,"Irregal Arguments.\n");
    }
    return;
}

/*--------------------------------------------------------------------------*
*  unalias機能の実装
 *--------------------------------------------------------------------------*/
void unalias(char *args[], struct com **a_top){
    struct com *now, *prev;
    
    if(arg_number == 2){
        now = *a_top;
        if(strcmp(now->command1, args[1])==0){
            *a_top = now->next;
            free(now);
            return;
        }
        prev = now;
        now = now->next;
        while(now != NULL){
            if(strcmp(now->command1, args[1])==0){
                prev -> next = now -> next;
                free(now);
                return;
            }
            prev = now;
            now = now->next;
        }
        fprintf(stderr, "The alias command not found.\n");
    }else{
        fprintf(stderr,"Irregal Arguments.\n");
    }
    return;
}

/*--------------------------------------------------------------------------*
*  alias_replace機能の実装
 *--------------------------------------------------------------------------*/
void alias_replace(char *args[], struct com **a_top){
    struct com *now;
    
    now = *a_top;
    while(now != NULL){
        if(strcmp(now->command1, args[0])==0){
            strcpy(args[0], now->command2);
            return;
        }
        now = now->next;
    }
    return;
}
/*-- END OF FILE -----------------------------------------------------------*/