#include <stdio.h>
#include "queue.h"

int queue_size (queue_t *queue){

  queue_t *aux = queue;

  // caso no qual a fila é vazia, retorne 0
  if(aux == NULL) return 0;

  int size = 1;

  // itere sobre a fila até que o primeiro elemento se repita
  // isto é, o 'loop' foi completo
  while(aux->next != queue){
    aux = aux->next;
    size++;
  }
  return size;
}


void queue_print (char *name, queue_t *queue, void print_elem (void*) ){

  queue_t *aux = queue;

  printf("%s: [", name);
  // caso no qual a fila não é vazia
  if(aux != NULL){
    // itere sobre a fila até retornar ao primeiro elemento
    // imprima cada elemento a partir do primeiro.
    while(aux->next != queue){
      print_elem(aux);
      printf(" ");
      aux = aux->next;
    }
    // imprime o ultimo elemento, que não foi incluido
    print_elem(aux);
  }
  printf("]\n");
}


int queue_append (queue_t **queue, queue_t *elem){
  queue_t *novo_elm = elem;

  // elemento não existe
  if(novo_elm == NULL){
    fprintf(stderr, "Elemento não existente");
    return -1;
  }

  // fila não existe
  if(queue == NULL){
    fprintf(stderr, "Fila não existente");
    return -1;
  }

  // elemento está contido em outra fila
  if(elem->next != NULL && elem->prev != NULL){
    fprintf(stderr, "Elemento contido em outra fila");
    return -1;
  }

  queue_t *first = *queue;
  queue_t *last = first;

  // caso fila vazia
  if(first == NULL){
    first = elem;
    first->next = first;
    first->prev = first;
    *queue = first;
    return 0;
  }

  // caso genérico
  // acha o ultimo elemento
  while(last->next != first) last = last->next;

  // ajusta os ponteiros
  last->next = elem;
  first->prev = elem;
  elem->next = first;
  elem->prev = last;
  last = elem;
  *queue = first;
  return 0;
}


int queue_remove (queue_t **queue, queue_t *elem){

  // elemento não existe
  if(elem == NULL){
    fprintf(stderr, "Elemento não existente");
    return -1;
  }
  // fila não existe
  if(queue == NULL){
    fprintf(stderr, "Fila não existente");
    return -1;
  }
  // fila vazia
  if(*queue == NULL){
    fprintf(stderr, "Fila vazia");
    return -1;
  }

  queue_t *first = *queue;
  queue_t *aux = first;

  // procura o elemento na fila
  while(aux->next != first && aux != elem) aux = aux->next;
  // caso ele não esteja contido, retorne <0
  if(aux != elem) return -1;

  // caso no qual a fila possui um único elemento
  // todos os ponteiros passam a apontar para NULL
  if(elem == *queue && elem->next == elem){
    elem->next = NULL;
    elem->prev = NULL;
    *queue = NULL;
    return 0;
  }

  // caso no qual o primeiro elemento é removido, ajustamos
  // o primeiro ponteiro para apontar para o próximo
  if(elem == *queue) *queue = (*queue)->next;

  // caso genérico aonde desfazemos os ponteiros do elemento removido
  elem->next->prev = elem->prev;
  elem->prev->next = elem->next;
  elem->next = NULL;
  elem->prev = NULL;
  return 0;
}
