// Francisco Teixeira de Barros, LETI (85069) & Mafalda Maria Ferreira de Vilar Gaspar, LEIC (77921)
// Projeto SO - exercicio 4, version 1.2, GRUPO 83
// Sistemas Operativos, DEI/IST/ULisboa 2016-17

#include "contas.h"

int deveTerminar = 0;		     
int contasSaldos[NUM_CONTAS];

pthread_mutex_t account_ctrl[NUM_CONTAS];

/**************************************************************************/
/*				Control de contas       					              */
/**************************************************************************/

void lock_account(int account) {
  /* confirmar que antes foi verificado que se trata de uma conta válida */
  if ((errno = pthread_mutex_lock(&account_ctrl[account-1])) != 0) {
    perror("Error in pthread_mutex_lock()");
    exit(EXIT_FAILURE);
  }
}

void unlock_account(int account) {
  /* confirmar que antes foi verificado que se trata de uma conta válida */
  if ((errno = pthread_mutex_unlock(&account_ctrl[account-1])) != 0) {
    perror("Error in pthread_mutex_unlock()");
    exit(EXIT_FAILURE);
  }
}

int contaExiste(int idConta) {
  return (idConta > 0 && idConta <= NUM_CONTAS);
}

/**************************************************************************/
/*				Inicilizacao de contas       				              */
/**************************************************************************/

void inicializarContas() {
  int i;
  for (i=0; i<NUM_CONTAS; i++){
    contasSaldos[i] = 0;
    if ((errno = pthread_mutex_init(&account_ctrl[i], NULL)) != 0) {
      perror("Error in pthread_mutex_init()");
      exit(EXIT_FAILURE);
    }
  }
}

/**************************************************************************/
/*				Terminacao de contas       					              */
/**************************************************************************/

void destroy_mutex_contas(){
  for(int i = 0; i < NUM_CONTAS; i++)
    pthread_mutex_destroy(&account_ctrl[i]);
}

/**************************************************************************/
/*				Manipulacao de contas       				              */
/**************************************************************************/

int debitarSemMutex(int idConta, int valor) {
  if (!contaExiste(idConta))
    return -1;

  if (contasSaldos[idConta - 1] < valor) {
    return -1;
  }
  contasSaldos[idConta - 1] -= valor;
  
  return 0;
}

int creditarSemMutex(int idConta, int valor) {
  if (!contaExiste(idConta))
    return -1;
    
  contasSaldos[idConta - 1] += valor;
  
  return 0;
}

/**************************************************************************/
/*				Comandos da aplicacao de contas       		              */
/**************************************************************************/

int debitar(int idConta, int valor) {
	int i;
	lock_account(idConta);
	i = debitarSemMutex(idConta, valor);
	unlock_account(idConta);
	return i;
}

int creditar(int idConta, int valor){
	int i;
	lock_account(idConta);
	i = creditarSemMutex(idConta, valor);
	unlock_account(idConta);
	return i;
}

int transferir(int idContaOrigem, int idContaDestino, int valor){
	if (!contaExiste(idContaOrigem) || !contaExiste(idContaDestino))
		return -1;

	if (idContaOrigem < idContaDestino) {
		lock_account(idContaOrigem);
		lock_account(idContaDestino);
	}
	else if (idContaOrigem > idContaDestino) {
		lock_account(idContaDestino);
		lock_account(idContaOrigem);
	}
	else
		return -1;

	if(debitarSemMutex(idContaOrigem, valor) < 0) {
		unlock_account(idContaOrigem);
		unlock_account(idContaDestino);
		return -1;
	}
	
	creditarSemMutex(idContaDestino, valor);
	
	unlock_account(idContaOrigem);
	unlock_account(idContaDestino);
	return 0;
}

int lerSaldo(int idConta) {
	int saldo;
	if (!contaExiste(idConta))
		return -1;

	lock_account(idConta);
	saldo = contasSaldos[idConta - 1];
	unlock_account(idConta);
	return saldo;
}


void trataSignal(int sig) {
	deveTerminar = 1;
}

void simular(int numAnos) {
	int id, saldo;
	int ano = 0;

	for (ano = 0; ano <= numAnos && !deveTerminar; ano++) {

		printf("SIMULAÇÃO: Ano %d\n=================\n", ano);

		for (id = 1; id<=NUM_CONTAS; id++) {
			if (ano > 0) {
				saldo = lerSaldo(id);
				creditar(id, saldo * TAXAJURO);
				saldo = lerSaldo(id);
				debitar(id, (CUSTOMANUTENCAO > saldo) ? saldo : CUSTOMANUTENCAO);
			}

			saldo = lerSaldo(id);
			while (printf("Conta %5d,\t Saldo %10d\n", id, saldo) < 0) {
				if (errno == EINTR)
					continue;
				else
					break;
			}
		}
	}

	if (deveTerminar)
		printf("Simulacao terminada por signal\n");  
}
