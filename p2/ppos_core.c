#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include "ppos.h"
#include "ppos_data.h"

#define STACKSIZE 64*1024

/*
  curr_task:  tarefa corrente
  main_task:  tarefa principal
  num_tasks:  numero de tarefas
*/

task_t *curr_task;
task_t main_task;
int num_tasks = 1;

void ppos_init(){
  // ponteiros da fila, para os da tarefa principal NULL
  main_task.next = NULL;
  main_task.prev = NULL;

  // ID da main 0 por default
  main_task.id = 0;

  // Salva contexto da tarefa main
  getcontext(&main_task.context);

  // A primeira tarefa corrente = main
  curr_task = &main_task;
  setvbuf(stdout, 0, _IONBF, 0);
}

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task, void (*start_func)(void *), void *arg){

  task->next = NULL;
  task->prev = NULL;
  char *stack;

  // guarda o contexto atual
  getcontext(&task->context);

  // aloca espaço para a pilha
  stack = malloc(STACKSIZE);
  if(stack)
   {
      task->context.uc_stack.ss_sp = stack ;
      task->context.uc_stack.ss_size = STACKSIZE ;
      task->context.uc_stack.ss_flags = 0 ;
      task->context.uc_link = 0 ;
   }
   else
   {
      perror ("Erro na criação da pilha: ") ;
      return 1;
   }

   // seta o endereço da funçao a ser executada e empilha parametros
   makecontext (&task->context, (void*)(*start_func), 1, arg);

   // atualiza id.
   task->id = num_tasks;
   num_tasks++;
/*
task :      descritor da nova tarefa
start_func: funcao corpo da tarefa
arg:        argumentos para a tarefa
*/
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exitCode){
  curr_task = &main_task;
  setcontext(&main_task.context);
}
// alterna a execução para a tarefa indicada
int task_switch (task_t *task){
  ucontext_t *aux_context = &curr_task->context;
  curr_task = task;
  swapcontext(aux_context, &task->context);
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id(){
  return (curr_task->id);
}
