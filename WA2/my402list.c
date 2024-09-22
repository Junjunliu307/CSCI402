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