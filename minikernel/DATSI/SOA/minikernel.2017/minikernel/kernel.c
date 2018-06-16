/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h"	/* Contiene defs. usadas por este modulo */

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	printk("-> NO HAY LISTOS. ESPERA INT\n");

	/* Baja al m�nimo el nivel de interrupci�n mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	while (lista_listos.primero==NULL)
		espera_int();		/* No hay nada que hacer */





	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no deber�a llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");


	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	char car;

	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL %c\n", car);

        return;
}

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){

	printk("-> TRATANDO INT. DE RELOJ\n");


	BCP *proceso_listo = lista_listos.primero;
	
	/* PARTE TIEMPOS_PROCESO A�adimos contadores usuario o a sistema para el proceso en ejecuci�n. Si es nulo nada.*/
	if(proceso_listo != NULL){
		if(viene_de_modo_usuario()){
			p_proc_actual->veces_usuario++;
		}
		else{
			p_proc_actual->veces_sistema++;
		}
	}



	// Incrementa contador de llamadas a int_reloj
	numTicks++;

	BCP *procesoADesbloquear = lista_bloqueados.primero;
	BCP *procesoSiguiente = NULL;
	if(procesoADesbloquear != NULL){
		procesoSiguiente = procesoADesbloquear->siguiente;
	}

	while(procesoADesbloquear != NULL){
		
		// Calculo de tiempo de bloqueo
		int numTicksBloqueo = procesoADesbloquear->secs_bloqueo * TICK;
		int ticksTranscurridos = numTicks - procesoADesbloquear->inicio_bloqueo;

		// Comprueba si el proceso se debe desbloquear
		if(ticksTranscurridos >= numTicksBloqueo){

			// Proceso de desbloquea y pasa a estado listo
			procesoADesbloquear->estado = LISTO;

			int nivel_interrupciones = fijar_nivel_int(NIVEL_3);
			eliminar_elem(&lista_bloqueados, procesoADesbloquear);
			insertar_ultimo(&lista_listos, procesoADesbloquear);
			fijar_nivel_int(nivel_interrupciones);	
		}

		procesoADesbloquear = procesoSiguiente;
		if(procesoADesbloquear != NULL){
			procesoSiguiente = procesoADesbloquear->siguiente;
		}
	}

    return;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciuones software
 */
static void int_sw(){

	printk("-> TRATANDO INT. SW\n");

	// Comprueba que proceso en ejecuci�n es el que se quiere bloquear
	if(idABloquear == p_proc_actual->id){
		// Pone el proceso ejecutando al final de la cola de listos
		BCP *proceso = lista_listos.primero;
		int nivel_interrupciones = fijar_nivel_int(NIVEL_3);
		eliminar_elem(&lista_listos, proceso);
		insertar_ultimo(&lista_listos, proceso);
		fijar_nivel_int(nivel_interrupciones);

		// Cambio de contexto por int sw de planificaci�n
		BCP *p_proc_bloqueado = p_proc_actual;
		p_proc_actual = planificador();
		cambio_contexto(&(p_proc_bloqueado->contexto_regs), &(p_proc_actual->contexto_regs));
	}

	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* rellenamos el BCP*/
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;

		/* lo inserta al final de cola de listos */
		int nivel_interrupciones = fijar_nivel_int(NIVEL_3);
		insertar_ultimo(&lista_listos, p_proc);
		fijar_nivel_int(nivel_interrupciones);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){

	printk("-> FIN PROCESO %d\n", p_proc_actual->id);

	liberar_proceso();

        return 0; /* no deber�a llegar aqui */
}

/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */



/** Devuelve el ID del proceso por el cual es invocado **/

int sis_obtener_id_pr() {
	return p_proc_actual->id;
}

/**Duerme el proceso los segundos especificados por registro**/
int sis_dormir(){

	unsigned int numSegundos;
	int nivel_interrupciones;
	numSegundos = (unsigned int)leer_registro(1);

	// actualiza BCP con el num de segundos y cambia estado a bloqueado
	p_proc_actual->estado = BLOQUEADO;
	p_proc_actual->inicio_bloqueo = numTicks;
	p_proc_actual->secs_bloqueo = numSegundos;	

	// Guarda el nivel anterior de interrupcion y lo fija a 3
	nivel_interrupciones = fijar_nivel_int(NIVEL_3);
	
	// 1. Saca de la lista de procesos listos el BCP del proceso
	eliminar_elem(&lista_listos, p_proc_actual);

	// 2. Inserta el BCP del proceso en la lista de bloqueados
	insertar_ultimo(&lista_bloqueados, p_proc_actual);

	// Restaura el nivel de interrupcion anterior
	fijar_nivel_int(nivel_interrupciones);

	// Cambio de contexto voluntario
	BCP *p_proc_dormido = p_proc_actual;
	p_proc_actual = planificador();
	cambio_contexto(&(p_proc_dormido->contexto_regs), &(p_proc_actual->contexto_regs));

	return 0;


}

/*devuelve el n�mero de interrupciones de reloj que se han producido desde que arranc� el sistema. */
int sis_tiempos_proceso(){

	struct tiempos_ejec *tiempos_ejecucion;

	// Comprueba si existe argumento
	tiempos_ejecucion = (struct tiempos_ejec *)leer_registro(1);

	if(tiempos_ejecucion != NULL){
		// Si hay argumento fija variable global
		int nivel_interrupciones = fijar_nivel_int(NIVEL_3);
		accesoParam = 1;
		fijar_nivel_int(nivel_interrupciones);

		// Rellena estructura con el tiempo de sistema y tiempo de usuario
		tiempos_ejecucion->sistema = p_proc_actual->veces_sistema;
		tiempos_ejecucion->usuario = p_proc_actual->veces_usuario;
	}

	return numTicks;
}




int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}
