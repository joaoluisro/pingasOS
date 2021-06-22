// GRR20186983 João Luis Ribeiro Okimoto
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include "ppos.h"
#include "ppos_data.h"

/*  -- Variaveis estáticas --

  STACKSIZE: tamanho da pilha alocada para uma tarefa
  PRIORITY: define a politica de prioridade das tarefas,
            0 -> FIFO, 1 -> prioridade por envelhecimento.
*/

#define STACKSIZE 64*1024
#define PRIORITY 1

/* -- Variaveis globais do sistema --
  curr_task:  tarefa corrente
  main_task:  tarefa principal
  num_tasks:  numero de tarefas criadas
  task_queue: fila de tarefas prontas
  user_tasks: numero de tarefas correntes
*/

task_t *curr_task, *task_queue;
task_t main_task, dispatcher;
unsigned int clock = 0;
int num_tasks = 1, user_tasks = 0;

/* -- Estruturas de interrupção --
  action:  ação que trata um sinal
  timer:   um temporizador que emite uma interrupção de forma ciclica
*/

struct sigaction action;
struct itimerval timer;

// chamada do scheduler
task_t *scheduler(){
  task_t *aux = task_queue;
  task_t *next_task = aux;
  // próxima tarefa é a que tiver maior prioridade de acordo com a politica
  if(PRIORITY){
    // percorre a fila procurando a tarefa com maior prio
    int highest_prio = aux->dynamic_prio;
    while(aux->next != task_queue){
      aux = aux->next;

      // achei uma prioridade maior
      if(aux->dynamic_prio < highest_prio){
        highest_prio = aux->dynamic_prio;
        next_task = aux;
      }
    }

    // implementa envelhecimento de tarefas
    aux = task_queue;
    while(aux->next != task_queue){
      if(aux != next_task) aux->dynamic_prio--;
      aux = aux->next;
    }

    // retorna o valor da prioridade estatica para a tarefa escolhida
    next_task->dynamic_prio = task_getprio(next_task);
  }
  // proxima tarefa é a primeira da fila de acordo com a politica FIFO
  else{
    next_task = task_queue;
  }
  // remove a tarefa da fila de prontas para executa-la
  if(task_queue != NULL) queue_remove((queue_t **)&task_queue, (queue_t *)next_task);
  return next_task;
}

// chamada do dispatcher
void dispatcher_call(){
  task_t *next_task;

  // enquanto houverem tarefas correntes
  while(user_tasks > 0){

    // pegue próxima tarefa
    next_task = scheduler();

    // coloque a tarefa para executar e configure seu quantum
    if(next_task != NULL){
      next_task->quantum = 20;
      task_switch(next_task);
    }
  }
  task_exit(0);
}

// chamada de interrupção
void interrupt_handler(){
  clock++;
  // se a tarefa corrente é dispatcher ou main, ignore
  if(curr_task == &dispatcher) return;

  // se o quantum máximo da tarefa expirou, coloque ela no final da fila
  // e retorne o fluxo para o dispatcher
  if(curr_task->quantum == 0){
    if(task_queue != NULL) queue_remove((queue_t **)&task_queue, (queue_t *)curr_task);
    queue_append((queue_t **)&task_queue, (queue_t *)curr_task);
    curr_task->activations++;
    dispatcher.activations++;
    task_switch(&dispatcher);
    return;
  }

  // caso seja uma tarefa do usuario
  curr_task->ps_time++;
  curr_task->quantum--;
}

// inicialização do PingasOS
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
  user_tasks++;
  queue_append((queue_t **)&task_queue, (queue_t *)curr_task);
  setvbuf(stdout, 0, _IONBF, 0);

  // cria o dispatcher
  task_create(&dispatcher, (void*)(*dispatcher_call), NULL);

  // configura a ação de tratamento da interrupção
  action.sa_handler = interrupt_handler ;
  sigemptyset (&action.sa_mask) ;
  action.sa_flags = 0 ;
  sigaction (SIGALRM, &action, 0);

   // configura valores do temporizador para disparar a cada 1ms
   timer.it_value.tv_usec = 1000 ;              // primeiro disparo, em micro-segundos
   timer.it_value.tv_sec  = 0 ;                 // primeiro disparo, em segundos
   timer.it_interval.tv_usec = 1000 ;           // disparos subsequentes, em micro-segundos
   timer.it_interval.tv_sec  = 0 ;              // disparos subsequentes, em segundos
   setitimer (ITIMER_REAL, &timer, 0);
}

unsigned int systime(){
  return clock;
}

// define a prioridade estática de uma tarefa (ou a tarefa atual)
void task_setprio (task_t *task, int prio){
  // coloque um limite no valor de prioridade
  if(prio < -20) prio = -20;
  if(prio > 20) prio = 20;

  // mude a prioridade, caso não seja especificada, mude a da tarefa atual
  if(task != NULL){
    task->static_prio = prio;
    task->dynamic_prio = prio;
  }
  else{
     curr_task->static_prio = prio;
     curr_task->dynamic_prio = prio;
  }
}


// retorna a prioridade estática de uma tarefa (ou a tarefa atual)
int task_getprio (task_t *task){
  if(task != NULL) return task->static_prio;
  else return curr_task->static_prio;
}


void task_yield(){
  // Se a tarefa atual for de usuário, colocamos-a no final da fila
  if(task_queue != NULL) queue_remove((queue_t **)&task_queue, (queue_t *)curr_task);
  queue_append((queue_t **)&task_queue, (queue_t *)curr_task);

  // fluxo volta para o dispatcher
  task_switch(&dispatcher);
}


// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task, void (*start_func)(void *), void *arg){
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

   // seta id e prioridade
   task->id = num_tasks;
   task->static_prio = 0;
   task->dynamic_prio = task->static_prio;
   num_tasks++;

   // seta tempo de processamento e de ativações
   task->ps_time = 0;
   task->start_time = systime();
   task->activations = 0;

   // se a tarefa for de usuario, adicione-a na fila e incremente o contador
   if(task != &dispatcher){
     user_tasks++;
     queue_append((queue_t **)&task_queue, (queue_t *)task);
   }

   return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exitCode){

  // se ela for de usuario, atualize o contador
  user_tasks--;

  printf ("Task %d exit: execution time %4d ms, processor time %4d ms, %d activations \n", curr_task->id, systime() - curr_task->start_time, curr_task->ps_time, curr_task->activations) ;

  if(curr_task == &dispatcher) return;
  // retorne para dispatcher caso seja de usuario
  task_switch(&dispatcher);
}


// alterna a execução para a tarefa indicada
int task_switch (task_t *task){
  ucontext_t *aux_context = &curr_task->context;
  curr_task = task;
  swapcontext(aux_context, &task->context);
  return 1;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id(){
  return (curr_task->id);
}
