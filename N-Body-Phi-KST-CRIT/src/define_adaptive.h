//*****************************************************************************************************
// Modificado por Juan F. Sanjuan Estrada
// METODO: Establece le procedimiento para adaptarse
#define	NONADAPTABLE	0 // Usuario establece un numero estatico de hebras
#define	ACW				1 // A nivel de aplicación calcula su productividad
#define	AST				2 // A nivel de aplicación, lee los ficheros del /proc
#define APW				3 // A nivel de aplicación, lee los ficheros del /sys/class/micras/power
#define	SIGNALS			3 // ???
#define KST				4 // La aplicación realiza llamadas al sistema, que chequean el criterio de decisión.
#define KITST			5 // La hebra ociosa del kernel chequea periodicamente, y la aplicación realiza una llamada al sistema para informarse. 
#define SST				6 // El scheduler chequea periodicamente, y la aplicación realiza una llamada al sistema para informarse.

// CRITERIO: Establece el criterio para decidir cuando se crea una nueva hebra

#define TEST_APPLICATION				0 // Permite analizar el funcionamiento de la aplicación con distinto numero de hebras activas
#define KERNEL_NONADAPTABLE				1

#define NOSLEEPTHREAD					2 // Si no se ha dormido ninguna hebra.
#define NO_IBT							3 
#define NO_NIBT							4 
#define NO_WAITING						5 

#define	SLEEPTIMEvsRUNTIME				6 //
#define SLEEPTIMEvsINTERVAL				7
#define SLEEPTIMEvsINI_INTERVAL			8

#define MNT_IBTvsRUNTIME				9
#define MNT_IBTvsINTERVAL				10
#define MNT_IBTvsINI_INTERVAL			11

#define MNT_NIBTvsRUNTIME				12
#define MNT_NIBTvsINTERVAL				13
#define MNT_NIBTvsINI_INTERVAL			14

#define MNT_WAITINGvsRUNTIME			15
#define MNT_WAITINGvsINTERVAL			16
#define MNT_WAITINGvsINI_INTERVAL		17

#define MNT_BTvsRUNTIME					18
#define MNT_BTvsINTERVAL				19			
#define MNT_BTvsINI_INTERVAL			20	

#define PREDICTOR						21 // Si la predicción de la suma del runtime de todas las hebras es mayor, o igual, que el real. 

#define CPUIDLE							100 // Si exite alguna(s) cpus ociosas.

