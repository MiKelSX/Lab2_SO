Laboratorio 2 - Sistemas Operativos

Simulador de Supervivencia Ártico

Integrantes:

    Miguel Cornejo, Rut: 21.372.437-1

    Diego Lobos, Rut: 21.799.105-6

Descripción

Programa en C++ que simula un escenario de supervivencia en el Ártico, donde un grupo de 4 equipos debe colaborar para recolectar recursos esenciales durante un número determinado de días. La simulación utiliza multiprocesamiento para que cada equipo opere de forma concurrente y memoria compartida como mecanismo de comunicación inter-proceso (IPC) para que un proceso coordinador centralice los resultados y evalúe el estado del grupo. El objetivo es sobrevivir manteniendo la moral y logrando el rescate.

Bibliotecas Utilizadas

    iostream: Para operaciones de entrada/salida y mostrar el estado de la simulación en consola.

    cstdlib: Para la función atoi, que convierte el argumento de días de simulación de texto a número.

    unistd.h: Para funciones estándar de POSIX como fork() (crear procesos), sleep() y usleep() (pausas en la ejecución).

    sys/wait.h: Para la función waitpid(), que permite al proceso padre esperar la finalización de sus hijos.

    sys/ipc.h y sys/shm.h: Para la gestión de la memoria compartida (shmget, shmat, shmdt, shmctl).

    ctime y random: Para la generación de números aleatorios con semillas complejas, asegurando resultados variables en cada ejecución.

    string y vector: Para la manipulación de cadenas de texto y listas, como los nombres de los equipos y sus actividades.

    cstring: Para la función memset, usada para inicializar la memoria compartida en ceros.

    errno.h: Para el manejo y reporte de errores del sistema.

Especificación de Algoritmos y Desarrollo

1. Algoritmo de Coordinación de Procesos (Proceso Padre)

    Entrada: Número de días a simular (ingresado por el usuario).

    Proceso:

        Crea un segmento de memoria compartida usando shmget().

        Inicia un bucle principal que itera por cada día de la simulación.

        En cada día, crea 4 procesos hijos (equipos) usando fork().

        Espera activamente (polling) a que todos los hijos reporten su finalización a través de un contador en la memoria compartida.

        Una vez finalizados, lee los recursos recolectados por cada equipo desde la memoria compartida.

        Evalúa si se cumplen los requisitos mínimos de supervivencia y aplica penalizaciones a la moral si es necesario.

        Verifica las condiciones de victoria (10 días de señales consecutivas) o derrota (moral llega a 0).

        Al final de la simulación, libera la memoria compartida con shmctl().

    Complejidad: O(d), donde d es el número total de días de simulación.

2. Algoritmo de Tareas de Equipo (Proceso Hijo)

    Entrada: Identificador del equipo (0-3) y el ID del segmento de memoria compartida.

    Proceso:

        Se enlaza al segmento de memoria compartida creado por el padre usando shmat().

        Reporta su PID y estado inicial ("trabajando") en la memoria compartida.

        Simula fases de trabajo (exploración, recolección, finalización) usando pausas con sleep().

        Calcula el éxito de su misión y la cantidad de recursos obtenidos mediante funciones de generación aleatoria.

        Escribe los resultados finales (recursos y estado "completado") en la memoria compartida.

        Incrementa de forma atómica el contador de equipos finalizados.

        Se desvincula de la memoria compartida con shmdt() y termina su ejecución con _exit(0).

    Complejidad: O(1) por cada día, ya que las operaciones son de tiempo constante.

3. Algoritmo de Generación de Resultados Aleatorios

    Entrada: Día actual e ID del equipo.

    Proceso:

        Construye una semilla única combinando random_device, el tiempo actual (time), el PID del proceso y los parámetros de entrada para garantizar alta variabilidad.

        Utiliza el generador mt19937 para producir números con una distribución uniforme.

        Determina un porcentaje de éxito basado en probabilidades predefinidas (30% éxito total, 50% parcial, 20% fracaso).

        Calcula las unidades de recursos finales aplicando el porcentaje de éxito a un rango base específico para cada tipo de equipo.

    Complejidad: O(1) por llamada.

Desarrollo Realizado

    Simulación Concurrente: Implementación de un modelo padre-hijo con fork() donde 4 procesos hijos operan en paralelo.

    Comunicación Inter-Proceso (IPC): Uso de memoria compartida para que los hijos reporten resultados al padre de forma eficiente.

    Gestión de Estado: El proceso padre centraliza y gestiona el estado global de la simulación (días, moral, señales consecutivas).

    Generación Aleatoria Robusta: Sistema de aleatoriedad con semillas complejas para evitar resultados repetitivos entre ejecuciones.

    Manejo de Argumentos: El programa acepta el número de días como argumento de línea de comandos o lo solicita de forma interactiva.

    Lógica de Supervivencia: Implementación de reglas claras de victoria, derrota y penalización de recursos.

Supuestos Utilizados

    La simulación se ejecuta en un entorno tipo UNIX (Linux, macOS) que soporte las llamadas al sistema fork y shmget.

    El número de equipos está fijado en 4, cada uno con una especialización de recursos predefinida.

    Las constantes de supervivencia (recursos mínimos, moral inicial, etc.) están definidas en el código y no son modificables en tiempo de ejecución.

    El proceso padre espera a que los 4 hijos terminen su ciclo diario antes de procesar los resultados y pasar al siguiente día.

    La comunicación es unidireccional para los resultados: los hijos escriben y el padre lee.

Instrucciones de Compilación

Bash

make

Instrucciones de Ejecución

El programa puede ejecutarse de dos maneras:

    Modo con argumento (especificando los días):
    Bash

make run ARGS="20"

o directamente:
Bash

./LAB2_Cornejo_Lobos 20

(Donde 20 es un número de días entre 10 y 30)

Modo interactivo (el programa solicitará los días):
Bash

make run

o directamente:
Bash

    ./LAB2_Cornejo_Lobos

Flujo de Trabajo Completo

    Compilar el programa: make

    Ejecutar la simulación: make run o ./LAB2_Cornejo_Lobos [dias]

    Observar los resultados: Toda la salida de la simulación se mostrará en la consola, día por día.

    Limpiar: make clean para eliminar el archivo ejecutable.

Comandos del Makefile

    make: Compila el código fuente LAB2_Cornejo_Lobos.cpp y genera el ejecutable.

    make run: Ejecuta el programa. Permite pasar argumentos con ARGS.

    make clean: Elimina el archivo ejecutable generado.

    make help: Muestra una ayuda simple con los comandos disponibles.

Estructura del Proyecto

Laboratorio_2/
├── LAB2_Cornejo_Lobos.cpp     # Código fuente principal
├── Makefile                   # Archivo de compilación y ejecución
└── README.md                  # Este archivo

Requisitos del Sistema

    Compilador C++: Compatible con C++11 o superior (ej. g++).

    Sistema Operativo: Linux o un sistema operativo compatible con POSIX.

    Bibliotecas: Biblioteca estándar de C++ y las cabeceras del sistema para IPC.

Consideraciones Importantes

    El resultado de cada simulación es no determinista debido al uso de generación de números aleatorios. Cada ejecución producirá un resultado diferente.

    El número de días a simular debe estar obligatoriamente en el rango de 10 a 30. El programa validará esta entrada.

    El programa gestiona la creación y destrucción de la memoria compartida. En caso de una interrupción abrupta, podría quedar un segmento de memoria huérfano en el sistema.