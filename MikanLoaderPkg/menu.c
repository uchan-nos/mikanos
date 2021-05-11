#include <Uefi.h>
#include <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include "menu.h"

/* EFI_INPUT_KEY poling_key(); */
void logo_print(int cursor_x, int cursor_y);
void clear();
void menu_add(int menu_number, EFI_STRING menu_name, int menu_type);
void set_cursor(int cursor_x, int cursor_y);


void boot_menu();

void boot_menu(){
    boot_menu_print();
}

/* メニューに追加を行う場合、ここに画面に表示をするメニューを描く必要があります。*/
/* menuadd関数にメニューのキーと表示名、メインメニューかOptionメニューか？を追加すれば追加されます、 */
/* ですが、Optionsは場所が不安定なので、setcursorをいちいちする必要があります。 */
void boot_menu_print(){
    clear();
    logo_print(0, 0);
    set_cursor(0, 10);
    Print(L"Hello, Mikan World!\n");
    set_cursor(0, 11);
    Print(L"========================================");
    menu_add(1, L"mikanos boot", 1);


    /* Optionの表示 */
    set_cursor(5, 15);
    Print(L"Options");
    set_cursor(5, 16);
    menu_add(2, L"UEFI information", 2);
    Print(L"\n========================================\n");    
}

void set_cursor(int cursor_x, int cursor_y){
        gST->ConOut->SetCursorPosition(gST->ConOut, cursor_x, cursor_y);
}

/* menu_typeは1がメインメニュー関連、2がOption関連 */
/* Optionだとset_cursorをいちいちする必要があります。 */
void menu_add(int menu_number, EFI_STRING menu_name, int menu_type){
    if (menu_type == 1){
        set_cursor(5, 12 + menu_number);
        Print(L"%d, ", menu_number);
        Print(L"%s\n", menu_name);
    } else if (menu_type == 2) {
        Print(L"%d, ", menu_number);
        Print(L"%s\n", menu_name);
    } else {
        Print(L"bad menu type\n");
    }
}

void clear(){
    gST->ConOut->ClearScreen(gST->ConOut);
}

/* menuとして、mikanOSの通常ブートを行う場合はreturnで呼び出し元に処理を戻す */
EFI_INPUT_KEY poling_key(){
    EFI_INPUT_KEY retkey = {0, 0};
    EFI_STATUS status;
    UINTN index = 0;

    /* status = gBS->WaitForEvent(1, &(gST->ConIn->WaitForKey), &index); */

    status = gBS->WaitForEvent(1, &(gST->ConIn->WaitForKey), &index);

    if (!EFI_ERROR(status)) {
        if (index == 0) {
            EFI_INPUT_KEY get_key;
            status = gST->ConIn->ReadKeyStroke(gST->ConIn, &get_key);
            if (!EFI_ERROR(status)) {
                retkey = get_key;
            }
        }
    }
    return retkey;
}


void logo_print(int cursor_x, int cursor_y){
  gST->ConOut->SetCursorPosition(gST->ConOut, cursor_x, cursor_y);
  Print(L"                                                                   _____  \n");
  Print(L"      ___    ___       _   _    _                        ____     / ___ ) \n");
  Print(L"     /   |  |   \\     |_| | |  / /           _  ___     / __ \\   ( (_     \n");
  Print(L"    / _  |  |  _ \\     _  | | / /  _____    | |/ _ \\   / /  \\ \\   \\_ \\    \n");
  Print(L"   / / \\ |  | / \\ \\   | | | |/ /  |  _  |   |  /  \\ \\ | |    | |    \\ \\   \n");
  Print(L"  / /   \\ \\/ /   \\ \\  | | |  _ \\  | |_|  \\  | |    | | \\ \\__/ /   ___) )  \n");
  Print(L" /_/     \\__/     \\_\\ |_| |_| \\_\\ |_____|_\\ |_|    |_|  \\____/   (____/   \n");
}
