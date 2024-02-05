#ifndef __SUT_H__
#define __SUT_H__
#include <stdbool.h>
#include "queue.h"
#include <pthread.h>
#include <ucontext.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include "sut.h"

#define _XOPEN_SOURCE

#define STACK_SIZE 1024*1024

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t Eshutdown = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t IOshutdown = PTHREAD_MUTEX_INITIALIZER;


pthread_t *ext;
pthread_t *iot;

ucontext_t *mt;
ucontext_t *it;

struct queue readyQ;
struct queue waitQ;

struct timespec remaining, request = { 0, 1000*1000 };

int running = 1;
int done = 0;

char* Estack;
char* IOstack;


static void* E_EXEC(){
        getcontext(mt);
        while (running || queue_peek_front(&readyQ)!=NULL || queue_peek_front(&waitQ)!=NULL || pthread_mutex_trylock(&Eshutdown)!=0)
        {
                if(queue_peek_front(&readyQ)==NULL)
                {
                        pthread_mutex_unlock(&IOshutdown);
                        pthread_mutex_unlock(&lock);
                        nanosleep(&request,&remaining);
                }
                else
                {
                        pthread_mutex_trylock(&IOshutdown);

                        struct queue_entry *job = queue_pop_head(&readyQ);
                        pthread_mutex_unlock(&lock);
                        ucontext_t* uthread = (ucontext_t*)(job->data);
                        free(job);
                        swapcontext(mt, uthread);
                        if(done)
                        {
                                if(uthread->uc_stack.ss_sp!=NULL)
                                        free(uthread->uc_stack.ss_sp);
                                free(uthread);
                                done--;
                        }        
                }
                pthread_mutex_lock(&lock);
        }

        pthread_mutex_unlock(&IOshutdown);
        pthread_mutex_unlock(&lock);

        return NULL;
}

static void* IO_EXEC(){
        getcontext(it);
        while(running || queue_peek_front(&waitQ)!=NULL || queue_peek_front(&readyQ)!=NULL || pthread_mutex_trylock(&IOshutdown)!=0)
        {
                if(queue_peek_front(&waitQ)==NULL)
                {
                        pthread_mutex_unlock(&Eshutdown);
                        pthread_mutex_unlock(&lock);
                        nanosleep(&request,&remaining);
                }
                else
                {
                        pthread_mutex_trylock(&Eshutdown);

                        struct queue_entry *ioJob = queue_pop_head(&waitQ);
                        pthread_mutex_unlock(&lock);
                        ucontext_t* uthread = (ucontext_t*)ioJob->data;
                        free(ioJob);
                        swapcontext(it, uthread);    
                }
                pthread_mutex_lock(&lock);
        }

        pthread_mutex_unlock(&Eshutdown);
        pthread_mutex_unlock(&lock);
        
        return NULL;
}


typedef void (*sut_task_f)();

void sut_init(){
        ext = (pthread_t*)malloc(sizeof(pthread_t));

        iot = (pthread_t*)malloc(sizeof(pthread_t));

        mt = (ucontext_t *)malloc(sizeof(ucontext_t));
        it = (ucontext_t *)malloc(sizeof(ucontext_t));
        
        Estack = (char *)malloc(sizeof(char) * STACK_SIZE);
        IOstack = (char *)malloc(sizeof(char) * STACK_SIZE);

        mt->uc_stack.ss_sp = Estack;
        mt->uc_stack.ss_size = STACK_SIZE;

        it->uc_stack.ss_sp = IOstack;
        it->uc_stack.ss_size = STACK_SIZE;

        readyQ = queue_create();
        queue_init(&readyQ);
        waitQ = queue_create();
        queue_init(&waitQ);

        if(pthread_create(ext, NULL, E_EXEC, NULL)!=0){
                perror("pthread E_EXEC creation failed");
        }
        if(pthread_create(iot, NULL, IO_EXEC, NULL)!=0){
                perror("pthread IO_EXEC creation failed");
        }
}

bool sut_create(sut_task_f fn){
        ucontext_t *thr = (ucontext_t *)malloc(sizeof(ucontext_t));
        char* st = (char *)malloc(sizeof(char) * STACK_SIZE);
        getcontext(thr);

        thr->uc_stack.ss_sp = st;
        thr->uc_stack.ss_size = STACK_SIZE;
        thr->uc_stack.ss_flags = 0;
        thr->uc_link = mt;

        makecontext(thr, fn, 0);
        
        pthread_mutex_lock(&lock);
        struct queue_entry *node = queue_new_node(thr);
        queue_insert_tail(&readyQ,node);
        pthread_mutex_unlock(&lock);
}

void sut_yield(){
        ucontext_t *temp = (ucontext_t *)malloc(sizeof(ucontext_t));
        char* newStack = (char *)malloc(sizeof(char) * STACK_SIZE);

        temp->uc_stack.ss_sp = newStack;
        temp->uc_stack.ss_size = STACK_SIZE;
        temp->uc_stack.ss_flags = 0;
        temp->uc_link = mt;

        getcontext(temp);

        pthread_mutex_lock(&lock);
        struct queue_entry *node = queue_new_node(temp);
        queue_insert_tail(&readyQ,node);
        pthread_mutex_unlock(&lock);

        swapcontext(temp,mt);
}

void sut_exit(){
        done++;
}

int sut_open(char *file_name){
        ucontext_t *op = (ucontext_t *)malloc(sizeof(ucontext_t));
        char* ioStack = (char *)malloc(sizeof(char) * STACK_SIZE);

        op->uc_stack.ss_sp = ioStack;
        op->uc_stack.ss_size = STACK_SIZE;
        op->uc_stack.ss_flags = 0;

        pthread_mutex_lock(&lock);
        struct queue_entry *node = queue_new_node(op);
        queue_insert_tail(&waitQ, node);
        pthread_mutex_unlock(&lock);

        swapcontext(op,mt);

        pthread_mutex_lock(&lock);
        int fd = open(file_name, O_RDWR | O_CREAT, 0644);
        struct queue_entry *newNode = queue_new_node(op);
        queue_insert_tail(&readyQ, newNode);
        pthread_mutex_unlock(&lock);

        swapcontext(op,it);

        return fd;
}

void sut_close(int fd){
        ucontext_t *op = (ucontext_t *)malloc(sizeof(ucontext_t));
        char* ioStack = (char *)malloc(sizeof(char) * STACK_SIZE);
        
        op->uc_stack.ss_sp = ioStack;
        op->uc_stack.ss_size = STACK_SIZE;
        op->uc_stack.ss_flags = 0;

        pthread_mutex_lock(&lock);
        struct queue_entry *node = queue_new_node(op);
        queue_insert_tail(&waitQ, node);
        pthread_mutex_unlock(&lock);

        swapcontext(op,mt);

        pthread_mutex_lock(&lock);
        close(fd);
        struct queue_entry *newNode = queue_new_node(op);
        queue_insert_tail(&readyQ, newNode);
        pthread_mutex_unlock(&lock);

        swapcontext(op,it);
}

void sut_write(int fd, char *buf, int size){
        ucontext_t *op = (ucontext_t *)malloc(sizeof(ucontext_t));
        char* ioStack = (char *)malloc(sizeof(char) * STACK_SIZE);

        op->uc_stack.ss_sp = ioStack;
        op->uc_stack.ss_size = STACK_SIZE;
        op->uc_stack.ss_flags = 0;

        pthread_mutex_lock(&lock);
        struct queue_entry *node = queue_new_node(op);
        queue_insert_tail(&waitQ, node);
        pthread_mutex_unlock(&lock);

        swapcontext(op,mt);

        pthread_mutex_lock(&lock);
        write(fd,buf,size);
        struct queue_entry *newNode = queue_new_node(op);
        queue_insert_tail(&readyQ, newNode);
        pthread_mutex_unlock(&lock);

        swapcontext(op,it);
}

char* sut_read(int fd, char *buf, int size){
        ucontext_t *op = (ucontext_t *)malloc(sizeof(ucontext_t));
        char* ioStack = (char *)malloc(sizeof(char) * STACK_SIZE);

        op->uc_stack.ss_sp = ioStack;
        op->uc_stack.ss_size = STACK_SIZE;
        op->uc_stack.ss_flags = 0;

        pthread_mutex_lock(&lock);
        struct queue_entry *node = queue_new_node(op);
        queue_insert_tail(&waitQ, node);
        pthread_mutex_unlock(&lock);

        swapcontext(op,mt);

        pthread_mutex_lock(&lock);
        read(fd,buf,size);
        struct queue_entry *newNode = queue_new_node(op);
        queue_insert_tail(&readyQ, newNode);
        pthread_mutex_unlock(&lock);

        swapcontext(op,it);

        return buf;
}

void sut_shutdown(){
        running = 0;
        pthread_join(*iot,NULL);
        printf("IO ended\n");
        pthread_join(*ext,NULL);
        printf("Exe ended\n");

        if(mt->uc_stack.ss_sp!=NULL)
                free(mt->uc_stack.ss_sp);
        free(mt);

        if(it->uc_stack.ss_sp!=NULL)
                free(it->uc_stack.ss_sp);
        free(it);

        free(ext);
        free(iot);
}


#endif
