#include <iostream>       // Para entrada/salida estándar (cout, cin)
#include <cstdlib>        // Para funciones generales (atoi, exit)
#include <unistd.h>       // Para funciones POSIX (fork, sleep, usleep)
#include <sys/wait.h>     // Para waitpid y control de procesos hijos
#include <sys/ipc.h>      // Para IPC (Inter Process Communication)
#include <sys/shm.h>      // Para memoria compartida (shmget, shmat, shmdt)
#include <ctime>          // Para time (obtención de timestamp)
#include <cstring>        // Para memset (inicialización de memoria)
#include <string>         // Para std::string
#include <vector>         // Para std::vector
#include <random>         // Para generación de números aleatorios
#include <sys/types.h>    // Para tipos de datos del sistema (pid_t)
#include <errno.h>        // Para manejo de errores (perror)

using namespace std;

// ---------- Macros para bucles ----------
#define fo(i, n) for (i=0; i<n; i++)         // Bucle desde 0 hasta n-1
#define Fo(i, k, n) for (i=k; i<n; i++)      // Bucle desde k hasta n-1

//Estructuras de Datos
//Almacena la cantidad de recursos recolectados por todos los equipos (unidades)
struct Recursos {
    int agua;
    int alimentos;
    int construccion;
    int senales;
};


//Información reportada por cada equipo de recolección
struct ReporteEquipo {
    int id_equipo;             //Identificador del equipo (0-3)
    int recursos_recolectados; //Cantidad de unidades obtenidas por el equipo
    int estado_equipo;         //Estado actual del equipo (0: trabajando, 1: completado, -1: error)
    pid_t pid_equipo;          //PID del proceso que representa al equipo
};

//Estructura completa de la memoria compartida entre procesos
struct SharedMemoryData {
    ReporteEquipo reportes[4]; //Array o 'lista' de reportes de los 4 equipos
    int equipos_completados;  //Contador de equipos que han terminado su trabajo
    int dia_actual;           //Día actual de la simulación
};

//Constantes del Sistema
const int MIN_AGUA = 8;              //min de unidades de agua requeridas por día
const int MIN_ALIMENTOS = 12;         //min de unidades de alimentos requeridas por día
const int MIN_CONSTRUCCION = 4;       //min de unidades de construcción requeridas por día
const int MIN_SENALES = 2;            //min de unidades de señales requeridas por día
const int MORAL_INICIAL = 100;        //min de moral iniciales del grupo
const int DIAS_RESCATE = 10;          //dias consecutivos con señales para rescate exitoso
const int MAX_DIAS = 30;              //max número de días de simulación
const int MIN_DIAS = 10;              //min número de días de simulación


//Nombres de los equipos de recolección
const vector<string> nombres_equipos = {
    "AGUA", "ALIMENTOS", "CONSTRUCCION", "SEÑALES" };

//mensajes de actividades de exploración para cada equipo
const vector<string> actividades_exploracion = {
    "Explorando fuentes de agua...",              //equipo Agua
    "Explorando territorio para cazar...",        //equipo Alimentos
    "Buscando materiales de construcción...",     //equipo Construcción
    "Recolectando combustible seco..."  };        //equipo Señales

//mensajes de actividades de recolección para cada equipo
const vector<string> actividades_recoleccion = {
    "Recolectando agua del arroyo encontrado...", // Equipo Agua
    "Intentando pescar en la laguna...",          // Equipo Alimentos
    "Cortando ramas útiles...",                   // Equipo Construcción
    "Manteniendo fogata de señales..." };         // Equipo Señales


// Mensajes de actividades de finalización para cada equipo
const vector<string> actividades_finalizacion = {
    "Purificando agua recolectada...",            // Equipo Agua
    "¡Capturado pez pequeño! Preparando...",      // Equipo Alimentos
    "Construyendo refugio básico...",             // Equipo Construcción
    "Creando señales de humo..."      };          // Equipo Señales


//Funciones de Generación Aleatoria

//Genera un porcentaje de éxito aleatorio basado en probabilidades definidas
//dia actual de la simulación (influye en la semilla aleatoria)
//equipo_id Identificador del equipo (0-3) (influye en la semilla aleatoria)
//Porcentaje de éxito (100: éxito total, 50-80: éxito parcial, 5-29: fracaso)
//Probabilidades: 30% éxito total, 50% éxito parcial, 20% fracaso
 //Usa una combinación única de semillas para garantizar variabilidad entre ejecuciones

// Genera un porcentaje de éxito para un equipo en un día específico, simulando variabilidad realista
int generar_resultado(int dia, int equipo_id) {
    // Se construye una semilla única para cada proceso hijo, combinando:
    // - random_device: fuente de entropía del sistema (si está disponible)
    // - time(nullptr): tiempo actual en segundos
    // - getpid(): ID del proceso, desplazado para mayor dispersión
    // - dia y equipo_id: parámetros que introducen variabilidad contextual
    random_device rd; //std::random_device rd;
    unsigned int seed = rd() ^ static_cast<unsigned int>(time(nullptr)) ^
                        (getpid() << 8) ^ (dia * 31) ^ (equipo_id * 97); // formula de semilla

    // Se inicializa el generador Mersenne Twister con la semilla compuesta
    mt19937 gen(seed); //std::mt19937 gen(seed);

    // Se define una distribución uniforme entre 1 y 100 para simular probabilidad base
    uniform_int_distribution<> dis(1, 100); //std::uniform_int_distribution<> dis(1, 100);
    int probabilidad = dis(gen);

    // Se ajusta la probabilidad con un sesgo adicional dependiente del día y equipo,
    // asegurando que el resultado final esté en el rango [1, 100]
    probabilidad = (probabilidad + dia * 7 + equipo_id * 13) % 100 + 1;

    
    if (probabilidad <= 30) {//30% de probabilidad de éxito total (100%)
        return 100; // Éxito total
    } else if (probabilidad <= 80) {
        uniform_int_distribution<> partial_dis(50, 80);//50% de probabilidad de éxito parcial (entre 50% y 80%)
        return partial_dis(gen); // Éxito parcial
    } else {
        uniform_int_distribution<> fail_dis(5, 29);//20% de probabilidad de fracaso (entre 5% y 29%)
        return fail_dis(gen); // Fracaso
    }
}


//Calcula las unidades recolectadas basadas en el porcentaje de éxito y tipo de equipo
//porcentaje es de éxito obtenido (de 0 a 100)
//equipo_id Identificador del equipo (0-3) que determina el rango base
//dia, Día actual de la simulación (influye en la semilla aleatoria)
//int Cantidad de unidades recolectadas (siempre >= 0)

//Cada equipo tiene un rango diferente de unidades base:
//Agua: 6-14 unidades base
//Alimentos: 8-20 unidades base  
//Construcción: 4-12 unidades base
// Señales: 2-8 unidades base
int calcular_unidades(int porcentaje, int equipo_id, int dia) {
    random_device rd; // random_device para obtener entropía del sistema
    unsigned int seed = rd() ^ static_cast<unsigned int>(time(nullptr)) ^ 
                      (getpid() << 9) ^ (dia * 17) ^ (equipo_id * 41); // fórmula de semilla única
    mt19937 gen(seed); // Inicializar generador Mersenne Twister con la semilla

    int objetivo_base; // Unidades base antes de aplicar porcentaje y variación
    // Diferentes rangos por tipo de equipo para simular especialización
    switch (equipo_id) { //switch para diferentes equipos, en cada caso
        case 0: { 
            uniform_int_distribution<> dis(6, 14); // Equipo Agua: 6-14 unidades base
            objetivo_base = dis(gen); // Generar unidades base aleatorias
            break;  
        }
        case 1: { 
            uniform_int_distribution<> dis(10, 20); // Equipo Alimentos: 10-20 unidades base
            objetivo_base = dis(gen); 
            break; 
        }
        case 2: { // Equipo Construcción: 4-12 unidades base
            uniform_int_distribution<> dis(4, 12); 
            objetivo_base = dis(gen); 
            break; 
        }
        case 3: { // Equipo Señales: 2-8 unidades base
            uniform_int_distribution<> dis(2, 8);  
            objetivo_base = dis(gen); 
            break; 
        }
        default: objetivo_base = 5;  // Valor por defecto (no debería ocurrir), por seguridad y evitar warnings
    }

    // Aplicar porcentaje de éxito al objetivo base
    int unidades_finales = (objetivo_base * porcentaje) / 100; 
    
    // Pequeña variación aleatoria adicional (0 a +2 unidades)
    uniform_int_distribution<> bonus_dis(0, 2);
    unidades_finales += bonus_dis(gen);

    // Asegurar que no sea negativo (fallo absoluto puede dar 0 unidades)
    if (unidades_finales < 0) unidades_finales = 0;
    return unidades_finales;
}

//Función del Equipo (Proceso Hijo)
//Función ejecutada por cada proceso hijo (equipo de recolección)
//equipo_id Identificador del equipo (0-3)
//shm_id Identificador del segmento de memoria compartida
//dia Día actual de la simulación

//Cada equipo realiza tres fases: exploración, recolección y finalización
//Los resultados se escriben en la memoria compartida para que el coordinador los lea
//El proceso termina después de completar su trabajo y reportar resultados
void equipo_recoleccion(int equipo_id, int shm_id, int dia) {
    pid_t pid = getpid();  // Obtener PID único del proceso

    // Conectar a memoria compartida (usando el shmid pasado por el padre)
    SharedMemoryData* shared_data = (SharedMemoryData*) shmat(shm_id, NULL, 0); // el shared_data es un puntero a la estructura de memoria compartida
    if (shared_data == (void*) -1) {
        perror("shmat (hijo)");  // Error al conectar a memoria compartida
        exit(1); // Terminar proceso hijo en caso de error
    }

    // Inicializar reporte del equipo en memoria compartida
    // el shared_data es un puntero a la estructura de memoria compartida
    shared_data->reportes[equipo_id].id_equipo = equipo_id; // Identificador del equipo
    shared_data->reportes[equipo_id].estado_equipo = 0; // el Estado: trabajando
    shared_data->reportes[equipo_id].pid_equipo = pid; // el pid del equipo

    // Fase 1: Exploración
    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid << "] "
         << actividades_exploracion[equipo_id] << endl;

    // Tiempos de espera aleatorios para simular trabajo variable
    random_device rd; // random_device para obtener entropía del sistema
    unsigned int seed_sleep = rd() ^ static_cast<unsigned int>(time(nullptr)) ^ 
                            (pid << 5) ^ (dia * 13); // fórmula de semilla única
    mt19937 gen_sleep(seed_sleep); // Inicializar generador Mersenne Twister con la semilla
    uniform_int_distribution<> sleep_dis(1, 4);  // Espera entre 1-4 segundos

    sleep(sleep_dis(gen_sleep));  // Simular tiempo de exploración

    // Fase 2: Recolección
    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid << "] "
         << actividades_recoleccion[equipo_id] << endl;

    sleep(sleep_dis(gen_sleep));  // Simular tiempo de recolección

    // Fase 3: Finalización
    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid << "] "
         << actividades_finalizacion[equipo_id] << endl;

    sleep(1);  // Tiempo fijo para finalización

    // Generar resultado aleatorio único para este equipo en este día
    int porcentaje_exito = generar_resultado(dia, equipo_id);
    int unidades = calcular_unidades(porcentaje_exito, equipo_id, dia);

    // Escribir resultado final en memoria compartida
    shared_data->reportes[equipo_id].recursos_recolectados = unidades;
    shared_data->reportes[equipo_id].estado_equipo = 1;  // Estado: completado
    shared_data->reportes[equipo_id].pid_equipo = pid;

    // Incrementar contador atómicamente (evita condiciones de carrera)
    __sync_fetch_and_add(&shared_data->equipos_completados, 1);

    // Reporte final del equipo
    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid
         << "] Completo ciclo: " << unidades << " unidades obtenidas" << endl;

    // Desconectar de memoria compartida y terminar proceso
    if (shmdt(shared_data) == -1) {
        perror("shmdt (hijo)");  // Error al desconectar memoria compartida
    }
    _exit(0);  // Terminar proceso hijo
}

//Función de Evaluación de Supervivencia
//Evalúa si el grupo sobrevive el día basado en los recursos recolectados
//totales Recursos totales recolectados por todos los equipos
//moral_perdida [out] Cantidad de moral perdida (si no se cumplen mínimos)
//razon_penalizacion [out] Razón de la penalización de moral
//bool True si se cumplen todos los mínimos, False en caso contrario

//Sistema de penalizaciones:
//Agua faltante: -3 puntos por unidad faltante
//Alimentos faltantes: -2 puntos por unidad faltante  
//Construcción faltante: -4 puntos por unidad faltante
//Señales faltantes: -5 puntos por unidad faltante
bool evaluar_supervivencia(const Recursos& totales, int& moral_perdida, string& razon_penalizacion) {
    bool sobrevive = true;
    moral_perdida = 0;
    razon_penalizacion.clear();  // Limpiar string de razón
    vector<string> razones;      // Vector para acumular razones de penalización

    // Verificar agua
    if (totales.agua < MIN_AGUA) {
        int perdida = (MIN_AGUA - totales.agua) * 3;  // -3 por unidad faltante
        moral_perdida += perdida; // Acumular penalización
        razones.push_back("falta de agua");
        sobrevive = false; // No se cumplen los mínimos
    }
    
    // Verificar alimentos
    if (totales.alimentos < MIN_ALIMENTOS) {
        int perdida = (MIN_ALIMENTOS - totales.alimentos) * 2;  // -2 por unidad faltante
        moral_perdida += perdida; // Acumular penalización
        razones.push_back("falta de comida");
        sobrevive = false; // No se cumplen los mínimos
    }
    
    // Verificar construcción
    if (totales.construccion < MIN_CONSTRUCCION) {
        int perdida = (MIN_CONSTRUCCION - totales.construccion) * 1;  // -4 por unidad faltante
        moral_perdida += perdida; // Acumular penalización
        razones.push_back("falta de refugio");
        sobrevive = false; // No se cumplen los mínimos
    }
    
    // Verificar señales
    if (totales.senales < MIN_SENALES) {
        int perdida = (MIN_SENALES - totales.senales) * 2;  // -2 por unidad faltante
        moral_perdida += perdida; // Acumular penalización
        razones.push_back("señales insuficientes");
        sobrevive = false; // No se cumplen los mínimos
    }

    // Construir mensaje de penalización con formato natural
    if (!razones.empty()) { // Si hay razones de penalización
        razon_penalizacion = "por ";
        for (size_t i = 0; i < razones.size(); ++i) {
            if (i == 0) {
                razon_penalizacion += razones[i];
            } else if (i == razones.size() - 1) {
                razon_penalizacion += " y " + razones[i];
            } else {
                razon_penalizacion += ", " + razones[i];
            } // Formateo natural con comas y "y", agregando la razon
        }
    }
    
    return sobrevive; // Retornar si se cumplen todos los mínimos
}

//Función del Coordinador (Proceso Padre)

//Función principal que coordina toda la simulación
//dias_simulacion Número de días a simular (entre 10 y 30)

//Crea la memoria compartida, gestiona los procesos hijos y evalúa el progreso
//Controla las condiciones de victoria/derrota y muestra el estado de la simulación
void coordinador(int dias_simulacion) {
    int moral = MORAL_INICIAL;               // Moral inicial del grupo
    int senales_consecutivas = 0;            // Contador de días consecutivos con señales
    bool rescate_exitoso = false;            // Bandera de victoria por rescate

    // Encabezado de la simulación
    cout << "=== SISTEMA DE SUPERVIVENCIA ACTIVADO ===" << endl;
    cout << "Moral inicial: " << MORAL_INICIAL << "/100" << endl;
    cout << "Días de simulación: " << dias_simulacion << endl << endl;

    // Crear segmento de memoria compartida (IPC_PRIVATE evita colisiones de claves)
    int shm_id = shmget(IPC_PRIVATE, sizeof(SharedMemoryData), IPC_CREAT | 0600);
    if (shm_id == -1) { //si shm_id es -1, hubo un error
        perror("shmget (padre)");  // Error al crear memoria compartida
        exit(1);
    }

    // Conectar el proceso coordinador a la memoria compartida
    SharedMemoryData* shared_data = (SharedMemoryData*) shmat(shm_id, NULL, 0);
    if (shared_data == (void*) -1) { // si shared_data es un puntero inválido, hubo un error
        perror("shmat (padre)");  // Error al conectar a memoria compartida
        shmctl(shm_id, IPC_RMID, NULL);  // Intentar limpiar segmento
        exit(1);
    }

    // Inicializar memoria compartida a cero (eliminar basura previa)
    memset(shared_data, 0, sizeof(SharedMemoryData)); // desde la dirección de shared_data, poner 0s, tamaño de la estructura

    // Bucle principal de días de simulación
    for (int dia = 1; dia <= dias_simulacion && moral > 0 && !rescate_exitoso; ++dia) { // Mientras haya moral y no se haya rescatado
        cout << "=== DÍA " << dia << " DE SUPERVIVENCIA ===" << endl;
        cout << "Iniciando equipos de recolección..." << endl << endl;

        // Inicializar datos del día en memoria compartida
        shared_data->equipos_completados = 0;
        shared_data->dia_actual = dia;
        int i; fo(i, 4) {
            //shared_data->reportes[i] = {i, 0, 0, 0};
            shared_data->reportes[i].id_equipo = i;
            shared_data->reportes[i].recursos_recolectados = 0;
            shared_data->reportes[i].estado_equipo = 0;  // Estado: trabajando
            shared_data->reportes[i].pid_equipo = 0;
        }

        // Crear procesos hijos (equipos de recolección)
        vector<pid_t> pids(4, -1);  // Vector para almacenar PIDs de los hijos
        fo(i, 4) {
            pid_t pid = fork();  // Crear proceso hijo
            
            if (pid < 0) {
                // Error al crear proceso hijo
                perror("fork (padre)");
                
                // Esperar a los hijos ya creados antes de salir
                int j; fo (j, i) {
                    if (pids[j] > 0) waitpid(pids[j], NULL, 0);
                }
                
                // Limpiar memoria compartida
                shmdt(shared_data);
                shmctl(shm_id, IPC_RMID, NULL);
                exit(1);
            } else if (pid == 0) {
                // Código ejecutado por el proceso hijo
                equipo_recoleccion(i, shm_id, dia);
                // Nunca llega aquí (el hijo termina en equipo_recoleccion)
            } else {
                // Código ejecutado por el proceso padre
                pids[i] = pid;  // Almacenar PID del hijo
            }
        }

        // Esperar a que los 4 equipos completen su trabajo (polling responsivo)
        while (shared_data->equipos_completados < 4) {
            usleep(100000);  // Esperar 100ms (más responsivo que sleep(1))
        }

        // Recoger resultados de la memoria compartida
        Recursos recursos_dia = {0, 0, 0, 0};
        recursos_dia.agua = shared_data->reportes[0].recursos_recolectados;
        recursos_dia.alimentos = shared_data->reportes[1].recursos_recolectados;
        recursos_dia.construccion = shared_data->reportes[2].recursos_recolectados;
        recursos_dia.senales = shared_data->reportes[3].recursos_recolectados;

        // Esperar terminación de todos los procesos hijos (limpieza)
        fo (i, 4) {
            if (pids[i] > 0) waitpid(pids[i], NULL, 0);
        }

        // Mostrar reportes finales del día
        cout << "\nREPORTES FINALES:" << endl;
        cout << "[#] Equipo Agua (PID: " << shared_data->reportes[0].pid_equipo << "): "
             << recursos_dia.agua << " unidades obtenidas" << endl;
        cout << "[#] Equipo Alimentos (PID: " << shared_data->reportes[1].pid_equipo << "): "
             << recursos_dia.alimentos << " unidades obtenidas" << endl;
        cout << "[#] Equipo Construcción (PID: " << shared_data->reportes[2].pid_equipo << "): "
             << recursos_dia.construccion << " unidades obtenidas" << endl;
        cout << "[#] Equipo Señales (PID: " << shared_data->reportes[3].pid_equipo << "): "
             << recursos_dia.senales << " unidades obtenidas" << endl;

        // Mostrar resultados vs requisitos mínimos
        cout << "\nRESULTADOS DEL DÍA:" << endl;
        cout << "[i] Agua recolectada: " << recursos_dia.agua << "/" << MIN_AGUA << " ("
             << (recursos_dia.agua >= MIN_AGUA ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;
        cout << "[i] Alimentos obtenidos: " << recursos_dia.alimentos << "/" << MIN_ALIMENTOS << " ("
             << (recursos_dia.alimentos >= MIN_ALIMENTOS ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;
        cout << "[i] Materiales de construcción: " << recursos_dia.construccion << "/" << MIN_CONSTRUCCION << " ("
             << (recursos_dia.construccion >= MIN_CONSTRUCCION ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;
        cout << "[i] Señales mantenidas: " << recursos_dia.senales << "/" << MIN_SENALES << " ("
             << (recursos_dia.senales >= MIN_SENALES ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;

        // Evaluar supervivencia y calcular penalizaciones
        int moral_perdida;
        string razon_penalizacion;
        bool dia_sobrevivido = evaluar_supervivencia(recursos_dia, moral_perdida, razon_penalizacion);

        // Aplicar penalización a la moral
        moral -= moral_perdida;
        if (moral < 0) moral = 0;  // La moral nunca puede ser negativa

        // Actualizar contador de señales consecutivas
        if (recursos_dia.senales >= MIN_SENALES) {
            ++senales_consecutivas;
            cout << "[✓] Señales activas: Día " << senales_consecutivas << " consecutivo" << endl;
        } else {
            senales_consecutivas = 0;  // Reiniciar contador si no hay señales suficientes
            cout << "[✖] Señales insuficientes, contador reiniciado" << endl;
        }

        // Mostrar estado y moral actual
        cout << "\nEstado: " << (dia_sobrevivido ? "DÍA SUPERADO" : "SUPERVIVENCIA CRÍTICA") << endl;
        cout << "Moral del grupo: " << moral << "/100";
        if (moral_perdida > 0) {
            cout << " (-" << moral_perdida << " " << razon_penalizacion << ")";
        } else {
            cout << " (sin cambio)";
        }
        cout << endl;

        // Verificar condiciones de victoria/derrota
        if (senales_consecutivas >= DIAS_RESCATE) {
            // Victoria por rescate exitoso (ej: 10 días consecutivos con señales)
            cout << "\n██▓▒░ ¡RESCATE EXITOSO! ░▒▓██" << endl;
            cout << "Han mantenido señales activas por " << senales_consecutivas << " días consecutivos" << endl;
            rescate_exitoso = true;
            break;
        }
        
        if (moral <= 0) {
            // Derrota por moral agotada
            cout << "\n██▓▒░ FRACASO - MORAL AGOTADA ░▒▓██" << endl;
            cout << "El grupo ha perdido la esperanza después de " << dia << " días" << endl;
            break;
        }
        
        if (dia == dias_simulacion && !rescate_exitoso) {
            // Fin por límite de tiempo sin victoria/derrota
            cout << "\n██▓▒░ LÍMITE DE TIEMPO ALCANZADO ░▒▓██" << endl;
            cout << "Sobrevivieron " << dias_simulacion << " días con moral " << moral << "/100" << endl;
            cout << "Señales activas consecutivas: " << senales_consecutivas << "/" << DIAS_RESCATE << endl;
        }

        cout << "\n----------------------------------------\n" << endl;
        sleep(1);  // Pausa breve entre días para legibilidad
    }

    // Mensaje final de victoria (si aplica)
    if (rescate_exitoso) {
        cout << "\n██▓▒░ FELICIDADES - MISIÓN CUMPLIDA ░▒▓██" << endl;
        cout << "Todos los sobrevivientes han sido rescatados exitosamente" << endl;
    }

    // Limpieza final: desconectar y eliminar memoria compartida
    if (shmdt(shared_data) == -1) {
        perror("shmdt (padre)");  // Error al desconectar memoria compartida
    }
    
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl(IPC_RMID)");  // Error al eliminar segmento
    }
}

//Función principal que gestiona los argumentos y inicia la simulación
//argc Cantidad de argumentos
//argv Array de argumentos (argv[1] = días de simulación opcional)
//int Código de salida (0 = éxito, 1 = error)
 
//Si no se proporcionan argumentos, solicita interactivamente los días
//Valida que los días estén en el rango permitido (10-30)
int main(int argc, char* argv[]) {
    int dias_simulacion;

    if (argc == 2) {
        // Modo con argumento: ./programa <días>
        dias_simulacion = atoi(argv[1]);
        if (dias_simulacion < MIN_DIAS || dias_simulacion > MAX_DIAS) {
            cout << "Error: Los días deben estar entre " << MIN_DIAS << " y " << MAX_DIAS << endl;
            return 1;
        }
    } else if (argc == 1) {
        // Modo interactivo: solicitar días al usuario
        cout << "=== SISTEMA DE SUPERVIVENCIA ===" << endl;
        cout << "Ingrese el número de días a simular (" << MIN_DIAS << "-" << MAX_DIAS << "): ";
        cin >> dias_simulacion;
        
        if (cin.fail() || dias_simulacion < MIN_DIAS || dias_simulacion > MAX_DIAS) {
            cout << "Error: Debe ingresar un número entre " << MIN_DIAS << " y " << MAX_DIAS << endl;
            return 1;
        }
        cout << endl;
    } else {
        // Uso incorrecto: mostrar ayuda
        cout << "Uso: " << argv[0] << " [días_simulacion]" << endl;
        cout << "O ejecute sin argumentos para ingreso interactivo" << endl;
        cout << "Días deben estar entre " << MIN_DIAS << " y " << MAX_DIAS << endl;
        return 1;
    }

    // Iniciar simulación
    coordinador(dias_simulacion);
    return 0;
}