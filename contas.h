// Francisco Teixeira de Barros, LETI (85069) & Mafalda Maria Ferreira de Vilar Gaspar, LEIC (77921)
// Projeto SO - exercicio 4, version 1.2, GRUPO 83
// Sistemas Operativos, DEI/IST/ULisboa 2016-17

#ifndef CONTAS_H
#define CONTAS_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#define NUM_CONTAS 10
#define TAXAJURO 0.1
#define CUSTOMANUTENCAO 1

#define ATRASO 1

void inicializarContas();
void destroy_mutex_contas();
int contaExiste(int idConta);
int debitar(int idConta, int valor);
int debitarSemMutex(int idConta, int valor);
int creditar(int idConta, int valor);
int creditarSemMutex(int idConta, int valor);
int transferir(int idContaOrigem, int idContaDestino, int valor);
int lerSaldo(int idConta);
void trataSignal(int sig);
void simular(int numAnos);


#endif
