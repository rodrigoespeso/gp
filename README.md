# Planificador Adaptativo del Kernel (Kernel Adaptive Scheduler)

[ES]
Gracias al auge de las arquitecturas paralelas, las
aplicaciones multihebradas se han visto muy desarrolladas en estos
últimos años y, por lo tanto, han sido necesarias herramientas de ayuda
al desarrollador en la programación paralela para aprovechar la potencia
de los multiprocesadores y procesadores multihebrados. En el presente
trabajo se estudia como herramienta el Gestor del nivel de Paralelismo
(GP) a nivel de kernel, que ayuda a las aplicaciones multihebradas a
controlar el número de hebras en ejecución y, de esta manera, mejorar su
eficiencia. Durante el desarrollo del trabajo se realiza la integración del
GP en el núcleo Linux de la pila de software
Intel® Manycore Platform Software Stack (Intel® MPSS) del
coprocesador Intel® Xeon Phi. Además, se ha realizado la integración de
la librería Interfaz para el Gestor de Paralelismo (IGP) en una aplicación
existente para obtener una aplicación de benchmark, mediante la cual se
han realizado pruebas de rendimiento sobre este equipo para demostrar
el funcionamiento del GP a nivel de kernel.

[EN]
Due to the growth of the parallel architectures, also the multithreading
applications have been developed during the last years, so it was
needed tools which support the parallel programing in order to take
advantage of the potential of multiprocessors and multithreaded
processors. In this project, the "Gestor del nivel de Paralelismo " or "GP"
(paralelism level manager) is estudied at kernel level as a tool which
assist multithreaded applications to manage the number of threads in
execution and therefore, to develop its efficiency. In this project the GP is
integrated in the Linux kernel of the
Intel® Manycore Platform Software Stack (Intel® MPSS) installed
on the coprocessor Intel® Xeon Phi. Besides, the "Interfaz para el Gestor
de Paralelismo" or "IGP" library (paralelism manager interface)
was integrated into an existent application, in order to obtain a
benchmark through which tests have been executed on the coprocessor to
demonstrate the performance of the GP at kernel level.
