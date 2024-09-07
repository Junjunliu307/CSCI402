#include "cs402.h"
#include "my402list.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

int My402ListInit(My402List *list) {
    if (list == NULL) return FALSE;
    list->num_members = 0;
    list->anchor.next = &(list->anchor);
    list->anchor.prev = &(list->anchor);
    return TRUE;
}

int My402ListLength(My402List *list) {
    return list->num_members;
}

int My402ListEmpty(My402List *list) {
    return list->num_members == 0;
}

int My402ListAppend(My402List *list, void *obj) {
    My402ListElem *new_elem = (My402ListElem *)malloc(sizeof(My402ListElem));
    if (new_elem == NULL) return FALSE;

    new_elem->obj = obj;
    new_elem->next = &(list->anchor);
    new_elem->prev = list->anchor.prev;

    list->anchor.prev->next = new_elem;
    list->anchor.prev = new_elem;
    list->num_members++;

    return TRUE;
}

int My402ListPrepend(My402List *list, void *obj) {
    My402ListElem *new_elem = (My402ListElem *)malloc(sizeof(My402ListElem));
    if (new_elem == NULL) return FALSE;

    new_elem->obj = obj;
    new_elem->next = list->anchor.next;
    new_elem->prev = &(list->anchor);

    list->anchor.next->prev = new_elem;
    list->anchor.next = new_elem;
    list->num_members++;

    return TRUE;
}

void My402ListUnlink(My402List *list, My402ListElem *elem) {
    if (elem == NULL) return;

    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;

    free(elem);
    list->num_members--;
}

void My402ListUnlinkAll(My402List *list) {
    My402ListElem *elem = list->anchor.next;
    while (elem != &(list->anchor)) {
        My402ListElem *next_elem = elem->next;
        free(elem);
        elem = next_elem;
    }

    list->anchor.next = &(list->anchor);
    list->anchor.prev = &(list->anchor);
    list->num_members = 0;
}

int My402ListInsertBefore(My402List *list, void *obj, My402ListElem *elem) {
    if (elem == NULL) {
        return My402ListPrepend(list, obj);
    }

    My402ListElem *new_elem = (My402ListElem *)malloc(sizeof(My402ListElem));
    if (new_elem == NULL) return FALSE;

    new_elem->obj = obj;
    new_elem->next = elem;
    new_elem->prev = elem->prev;

    elem->prev->next = new_elem;
    elem->prev = new_elem;

    list->num_members++;
    return TRUE;
}

int My402ListInsertAfter(My402List *list, void *obj, My402ListElem *elem) {
    if (elem == NULL) {
        return My402ListAppend(list, obj);
    }

    My402ListElem *new_elem = (My402ListElem *)malloc(sizeof(My402ListElem));
    if (new_elem == NULL) return FALSE;

    new_elem->obj = obj;
    new_elem->next = elem->next;
    new_elem->prev = elem;

    elem->next->prev = new_elem;
    elem->next = new_elem;

    list->num_members++;
    return TRUE;
}

My402ListElem *My402ListFirst(My402List *list) {
    if (list->num_members == 0) return NULL;
    return list->anchor.next;
}

My402ListElem *My402ListLast(My402List *list) {
    if (list->num_members == 0) return NULL;
    return list->anchor.prev;
}

My402ListElem *My402ListNext(My402List *list, My402ListElem *elem) {
    if (elem->next == &(list->anchor)) return NULL; 
    return elem->next;
}

My402ListElem *My402ListPrev(My402List *list, My402ListElem *elem) {
    if (elem->prev == &(list->anchor)) return NULL; 
    return elem->prev;
}

My402ListElem *My402ListFind(My402List *list, void *obj) {
    My402ListElem *elem = NULL;
    for (elem = My402ListFirst(list); elem != NULL; elem = My402ListNext(list, elem)) {
        if (elem->obj == obj) return elem;
    }
    return NULL;
}

typedef struct {
    int timestamp;
    double amount;
    char description[1024];
} Transaction;

char* formatDate(int timestamp);
void formatAmount(char* buffer, double amount);
void formatBalance(char* buffer, double balance);
void formatDouble(char* buffer, double num);

int main(int argc, char *argv[]) {
    My402List transactionList;
    if (!My402ListInit(&transactionList)) {
        fprintf(stderr, "Error: Could not initialize list\n");
        return 1;
    }

    FILE *inputFile = fopen("test.tfile", "r");
    if (inputFile == NULL) {
        fprintf(stderr, "Error: Could not open input file\n");
        return 1;
    }

    char line[1024];
    char sign;
    while (fgets(line, sizeof(line), inputFile)) {
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

            My402ListElem *elem = NULL;
            for (elem = My402ListFirst(&transactionList); elem != NULL; elem = My402ListNext(&transactionList, elem)) {
                Transaction *currentTrans = (Transaction *)elem->obj;
                if (trans->timestamp < currentTrans->timestamp) {
                    break;
                }
            }
            if (elem == NULL) {
                My402ListAppend(&transactionList, trans);
            } else {
                My402ListInsertBefore(&transactionList, trans, elem);
            }
        }else{
            fprintf(stderr, "Error: malformed line\n");
            exit(EXIT_FAILURE);
        }
    }
    fclose(inputFile);

    printf("+-----------------+--------------------------+----------------+----------------+\n");
    printf("|       Date      | Description              |         Amount |        Balance |\n");
    printf("+-----------------+--------------------------+----------------+----------------+\n");

    double balance = 0.0;
    My402ListElem *elem = NULL;
    for (elem = My402ListFirst(&transactionList); elem != NULL; elem = My402ListNext(&transactionList, elem)) {
        Transaction *trans = (Transaction *)elem->obj;
        balance += trans->amount;

        char date[32], amount[16], balance_str[16], description[25];
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

char* formatDate(int timestamp) {
    static char buffer[32];
    struct tm *tm_info;
    time_t time = (time_t)timestamp;
    tm_info = localtime(&time);
    strftime(buffer, sizeof(buffer), "%a %b %d %Y", tm_info);
    return buffer;
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

// Function to format a double number into "1,333.02" format
void formatDouble(char* buffer, double num) {
    // Split the number into integer and decimal parts
    int int_part = (int)num;  // Get the integer part
    double decimal_part = num - int_part;  // Get the decimal part
    
    // Create a temporary buffer to store the integer part with commas
    char temp[32];
    int index = 0;
    int count = 0;
    
    // Handle the integer part and add commas
    while (int_part > 0) {
        if (count > 0 && count % 3 == 0) {
            temp[index++] = ',';
        }
        temp[index++] = '0' + (int_part % 10);
        int_part /= 10;
        count++;
    }
    
    if (index == 0) {
        temp[index++] = '0';  // In case the number is zero
    }
    
    temp[index] = '\0';
    reverse(temp);  // Reverse the string to get the correct order

    // Write the integer part to the buffer
    strcpy(buffer, temp);
    
    // Format the decimal part to two digits
    char decimal_str[10];
    sprintf(decimal_str, "%.2f", decimal_part);  // Format the decimal part
    strcat(buffer, decimal_str + 1);  // Skip the leading '0' in the decimal part
}