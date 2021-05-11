#include <Uefi.h>
#include <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include "menu.h"

EFI_INPUT_KEY poling_key();
void logo_print(int cursor_x, int cursor_y);
void clear();
void menu_add(int menu_number, EFI_STRING menu_name, int menu_type);
void set_cursor(int cursor_x, int cursor_y);
void boot_menu();
void print_device_information();
void boot_menu_print();

static EFI_INPUT_KEY get_key = {0, 0};

/* Optionで別画面を開くには、戻ってきた時に再度boot_menu_printをすればいい。 */
void boot_menu(){
    gST->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
    while (1) {
        boot_menu_print();
        get_key = poling_key();
        switch (get_key.UnicodeChar) {
        case '1':               /* mikanos boot */
            clear();
            return;
        case '2':
            print_device_information();
        }
    }
}

/*  */
void print_device_information(){
    EFI_INPUT_KEY exit_key = {0, 0};
    clear();
    Print(L"UEFI information \n");
    Print(L"UEFI Vendor information: %s\n", gST->FirmwareVendor);
    Print(L"UEFI Firmware version: 0x%x\n", gST->FirmwareRevision);
    Print(L"Support UEFI Specification: UEFI");
    switch (gST->Hdr.Revision) {
    case EFI_1_02_SYSTEM_TABLE_REVISION:
        Print(L" 1.02 ");
        break;
    case EFI_1_10_SYSTEM_TABLE_REVISION:
        Print(L" 1.10 ");
        break;
    case EFI_2_00_SYSTEM_TABLE_REVISION:
        Print(L" 2.00 ");
        break;
    case EFI_2_10_SYSTEM_TABLE_REVISION:
        Print(L" 2.10 ");
        break;
    case EFI_2_20_SYSTEM_TABLE_REVISION:
        Print(L" 2.20 ");
        break;
    case EFI_2_30_SYSTEM_TABLE_REVISION:
        Print(L" 2.30 ");
        break;
    case EFI_2_31_SYSTEM_TABLE_REVISION:
        Print(L" 2.31 ");
        break;
    case EFI_2_40_SYSTEM_TABLE_REVISION:
        Print(L" 2.40 ");
        break;
    case EFI_2_50_SYSTEM_TABLE_REVISION:
        Print(L" 2.50 ");
        break;
    case EFI_2_60_SYSTEM_TABLE_REVISION:
        Print(L" 2.60 ");
        break;
    case EFI_2_70_SYSTEM_TABLE_REVISION:
        Print(L" 2.70 ");
        break;
    case EFI_2_80_SYSTEM_TABLE_REVISION:
        Print(L" 2.80 ");
        break;
    default:
        Print(L"%x", gST->Hdr.Revision);
    }
    Print(L"supported\n");
    set_cursor(1, 15);
    Print(L"Return boot menu: Press q\n");
    while (1) {
        exit_key = poling_key();
        if (exit_key.UnicodeChar == 'q'){
            return;
        }
    }
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
    menu_add(2, L"Device information", 2);
    Print(L"\n========================================\n");    
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

void set_cursor(int cursor_x, int cursor_y){
        gST->ConOut->SetCursorPosition(gST->ConOut, cursor_x, cursor_y);
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
