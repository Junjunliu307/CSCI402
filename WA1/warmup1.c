/*
 * Author:      William Chia-Wei Cheng (bill.cheng@usc.edu)
 *
 * @(#)$Id: listtest.c,v 1.2 2020/05/18 05:09:12 william Exp $
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdbool.h>
#include "cs402.h"

#include "my402list.h"

typedef struct {
    int timestamp;
    double amount;
    char description[1024];
} Transaction;

void BubbleSortForwardList(My402List *pList, int num_items);
void BubbleForward(My402List *pList, My402ListElem **pp_elem1, My402ListElem **pp_elem2);
char* formatDate(int timestamp);
void formatAmount(char* buffer, double amount);
void formatBalance(char* buffer, double balance);
void formatDouble(char* buffer, double num);

int main(int argc, char *argv[]) {
    bool shouldSort = false;
    if (argc > 1 && strcmp(argv[1], "sort") == 0) {
        shouldSort = true;
    }
    My402List transactionList;
    if (!My402ListInit(&transactionList)) {
        fprintf(stderr, "Error: Could not initialize list\n");
        return 1;
    }

    // FILE *inputFile = fopen(argv[2], "r");
    // if (inputFile == NULL) {
    //     fprintf(stderr, "Error: Could not open input file\n");
    //     return 1;
    // }
    FILE *inputFile;
    if (argc > 2) {
        // 如果提供了文件路径，打开文件
        inputFile = fopen(argv[2], "r");
        if (inputFile == NULL) {
            fprintf(stderr, "Error: Could not open input file: %s\n", argv[2]);
            return 1;
        }
    } else {
        // 如果没有提供文件路径，从标准输入读取数据
        inputFile = stdin;
    }

    char line[1024];
    char tempBuffer[1024];
    char sign;
    while (fgets(line, sizeof(line), inputFile)) {
        // Check if the line exceeds 1024 characters
        if (line[strlen(line) - 1] != '\n' && !feof(inputFile)) {
            // Skip remaining characters of the long line
            while (fgets(tempBuffer, sizeof(tempBuffer), inputFile) && tempBuffer[strlen(tempBuffer) - 1] != '\n');
        
            fprintf(stderr, "Error: Line exceeds 1024 characters\n");
            fclose(inputFile);
            return 1;
        }
        Transaction *trans = (Transaction *)malloc(sizeof(Transaction));
        if (sscanf(line, "%c\t%d\t%lf\t%[^\n]", &sign, &trans->timestamp, &trans->amount, trans->description) == 4) {
            if (sign == '-') {
                trans->amount = -trans->amount;
            } else if(sign == '+'){
                trans->amount = trans->amount;
            }else{
                fprintf(stderr, "Error: malformed line\n");
                exit(EXIT_FAILURE);
            }
            My402ListAppend(&transactionList, trans);
        }else{
            fprintf(stderr, "Error: malformed line\n");
            exit(EXIT_FAILURE);
        }
    }
    fclose(inputFile);

    if (shouldSort) {
        int num_items = My402ListLength(&transactionList);
        BubbleSortForwardList(&transactionList, num_items);  // Sort by timestamp
    }

    printf("+-----------------+--------------------------+----------------+----------------+\n");
    printf("|       Date      | Description              |         Amount |        Balance |\n");
    printf("+-----------------+--------------------------+----------------+----------------+\n");

    double balance = 0.0;
    My402ListElem *elem = NULL;
    for (elem = My402ListFirst(&transactionList); elem != NULL; elem = My402ListNext(&transactionList, elem)) {
        Transaction *trans = (Transaction *)elem->obj;
        balance += trans->amount;

        char amount[16], balance_str[16], description[25];
        strncpy(description, trans->description, 24);
        description[24] = '\0';
        
        int len = strlen(description);
        for (int i = len; i < 24; i++) {
            description[i] = ' ';
        }
        description[24] = '\0';

        formatAmount(amount, trans->amount);
        formatBalance(balance_str, balance);

        printf("| %-15s | %-24s | %14s | %14s |\n", formatDate(trans->timestamp), description, amount, balance_str);
    }

    printf("+-----------------+--------------------------+----------------+----------------+\n");

    My402ListUnlinkAll(&transactionList);

    return 0;
}

void BubbleSortForwardList(My402List *pList, int num_items) {
    My402ListElem *elem = NULL;
    int i = 0;

    if (My402ListLength(pList) != num_items) {
        fprintf(stderr, "List length is not %1d in BubbleSortForwardList().\n", num_items);
        exit(1);
    }

    for (i = 0; i < num_items; i++) {
        int j = 0, something_swapped = FALSE;
        My402ListElem *next_elem = NULL;

        for (elem = My402ListFirst(pList), j = 0; j < num_items - i - 1; elem = next_elem, j++) {
            Transaction *cur_trans = (Transaction *)(elem->obj);
            Transaction *next_trans = NULL;

            next_elem = My402ListNext(pList, elem);
            if (next_elem != NULL) {
                next_trans = (Transaction *)(next_elem->obj);

                if (cur_trans->timestamp > next_trans->timestamp) {
                    BubbleForward(pList, &elem, &next_elem);
                    something_swapped = TRUE;
                }
            }
        }
        if (!something_swapped) break;
    }
}

void BubbleForward(My402List *pList, My402ListElem **pp_elem1, My402ListElem **pp_elem2) {
    My402ListElem *elem1 = (*pp_elem1), *elem2 = (*pp_elem2);
    void *obj1 = elem1->obj, *obj2 = elem2->obj;
    My402ListElem *elem1prev = My402ListPrev(pList, elem1);
    My402ListElem *elem2next = My402ListNext(pList, elem2);

    My402ListUnlink(pList, elem1);
    My402ListUnlink(pList, elem2);
    if (elem1prev == NULL) {
        (void)My402ListPrepend(pList, obj2);
        *pp_elem1 = My402ListFirst(pList);
    } else {
        (void)My402ListInsertAfter(pList, obj2, elem1prev);
        *pp_elem1 = My402ListNext(pList, elem1prev);
    }
    if (elem2next == NULL) {
        (void)My402ListAppend(pList, obj1);
        *pp_elem2 = My402ListLast(pList);
    } else {
        (void)My402ListInsertBefore(pList, obj1, elem2next);
        *pp_elem2 = My402ListPrev(pList, elem2next);
    }
}

char* formatDate(int timestamp) {
    static char formattedDate[32]; // buffer to store the formatted date
    char *ctimeStr;
    
    time_t rawtime = (time_t)timestamp;
    ctimeStr = ctime(&rawtime); // get ctime string

    // Remove the newline character at the end of ctime output
    ctimeStr[strlen(ctimeStr) - 1] = '\0';

    // Format the string to remove extra space before day if it's a single digit
    sprintf(formattedDate, "%.3s %.3s %.2s %.4s", ctimeStr, ctimeStr + 4, ctimeStr + 8, ctimeStr + 20);

    return formattedDate;
}

void formatAmount(char* buffer, double amount) {
    if (amount >= 10000000 || amount <= -10000000) {
        if (amount < 0) {
            snprintf(buffer, 16, "(?,???,???.??)");
        } else {
            snprintf(buffer, 16, " ?,???,???.?? ");
        }
    } else {
        if (amount < 0) {
            formatDouble(buffer,-amount);
            char temp[32]; 
            strcpy(temp, buffer);
            int blankLength = 14 - strlen(temp) - 2;
            snprintf(buffer, 16, "(%*s%s)",blankLength," ", temp);
        } else {
            formatDouble(buffer,amount);
            char temp[32]; 
            strcpy(temp, buffer);
            snprintf(buffer, 16, " %s ", temp);
        }
    }
}

void formatBalance(char* buffer, double balance) {
    if (balance >= 10000000 || balance <= -10000000) {
        snprintf(buffer, 16, "?,???,???.??");
    } else {
        if (balance < 0) {
            formatDouble(buffer,-balance);
            char temp[32]; 
            strcpy(temp, buffer);
            int blankLength = 14 - strlen(temp) - 2;
            snprintf(buffer, 16, "(%*s%s)",blankLength," ", temp);
        } else {
            formatDouble(buffer,balance);
            char temp[32]; 
            strcpy(temp, buffer);
            snprintf(buffer, 16, " %s ", temp);
        }
    }
}

// Helper function to reverse a string
void reverse(char* str) {
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = temp;
    }
}

void formatDouble(char* buffer, double num) {
    int int_part = (int)num;  // Integer part
    int decimal_part = (int)round((num - int_part) * 100);  // Two decimal places, rounded
    
    if (decimal_part == 100){
        int_part += 1;
        decimal_part = 0;
    }

    char temp[32];
    int index = 0;
    int count = 0;

    // Handle the integer part and add commas
    if (int_part == 0) {
        temp[index++] = '0';  // If the integer part is 0, ensure we print '0'
    } else {
        while (int_part > 0) {
            if (count > 0 && count % 3 == 0) {
                temp[index++] = ',';
            }
            temp[index++] = '0' + (int_part % 10);
            int_part /= 10;
            count++;
        }
    }
    
    temp[index] = '\0';
    reverse(temp);  // Reverse the string to get the correct order
    strcpy(buffer, temp);  // Write the integer part to the buffer

    // Format the decimal part with exactly two digits
    char decimal_str[10];
    snprintf(decimal_str, sizeof(decimal_str), ".%02d", decimal_part);  // Ensure two decimal places
    strcat(buffer, decimal_str);  // Concatenate decimal part to the buffer
}