/* LAB2_Apellido1_Apellido2.cpp
 *
 * Versión final corregida:
 * - Comunicación por memoria compartida (System V)
 * - Cada equipo reporta: id_equipo, recursos_recolectados, estado_equipo, pid_equipo
 * - Inicialización segura del segmento (memset)
 * - Seed local por proceso (sin variable global compartida)
 * - Polling más responsivo (usleep)
 * - Comprobaciones de retorno en shmdt/shmctl
 *
 * Reemplazar Apellido1_Apellido2 por apellidos reales antes de entregar.
 */

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>
#include <random>
#include <sys/types.h>
#include <errno.h>

using namespace std;

/* ---------- Estructuras ---------- */
struct Recursos {
    int agua;
    int alimentos;
    int construccion;
    int senales;
};

struct ReporteEquipo {
    int id_equipo;
    int recursos_recolectados;
    int estado_equipo; // 0: trabajando, 1: completado, -1: error
    pid_t pid_equipo;
};

struct SharedMemoryData {
    ReporteEquipo reportes[4];
    int equipos_completados;
    int dia_actual;
};

/* ---------- Constantes ---------- */
const int MIN_AGUA = 8;
const int MIN_ALIMENTOS = 12;
const int MIN_CONSTRUCCION = 4;
const int MIN_SENALES = 2;
const int MORAL_INICIAL = 100;
const int DIAS_RESCATE = 10;
const int MAX_DIAS = 30;
const int MIN_DIAS = 10;

/* ---------- Mensajes y nombres ---------- */
const vector<string> nombres_equipos = {
    "AGUA", "ALIMENTOS", "CONSTRUCCION", "SEÑALES"
};

const vector<string> actividades_exploracion = {
    "Explorando fuentes de agua...",
    "Explorando territorio para cazar...",
    "Buscando materiales de construcción...",
    "Recolectando combustible seco..."
};

const vector<string> actividades_recoleccion = {
    "Recolectando agua del arroyo encontrado...",
    "Intentando pescar en la laguna...",
    "Cortando ramas útiles...",
    "Manteniendo fogata de señales..."
};

const vector<string> actividades_finalizacion = {
    "Purificando agua recolectada...",
    "¡Capturado pez pequeño! Preparando...",
    "Construyendo refugio básico...",
    "Creando señales de humo..."
};

/* ---------- Funciones aleatorias ---------- */
/* Genera porcentaje de éxito: devuelve 100 (éxito total), 50-80 (parcial) o 5-29 (fracaso) */
int generar_resultado(int dia, int equipo_id) {
    // seed local por proceso: combinación de random_device + time + pid + dia + equipo
    std::random_device rd;
    unsigned int seed = rd() ^ static_cast<unsigned int>(time(nullptr)) ^ (getpid() << 8) ^ (dia * 31) ^ (equipo_id * 97);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(1, 100);

    int probabilidad = dis(gen);
    // variabilidad extra
    probabilidad = (probabilidad + dia * 7 + equipo_id * 13) % 100 + 1;

    if (probabilidad <= 30) {        // 30% éxito total
        return 100;
    } else if (probabilidad <= 80) { // 50% éxito parcial (50-80%)
        std::uniform_int_distribution<> partial_dis(50, 80);
        return partial_dis(gen);
    } else {                        // 20% fracaso (<30%)
        std::uniform_int_distribution<> fail_dis(5, 29);
        return fail_dis(gen);
    }
}

/* Calcula unidades según porcentaje y tipo de equipo. Mantengo la lógica previa. */
int calcular_unidades(int porcentaje, int equipo_id, int dia) {
    std::random_device rd;
    unsigned int seed = rd() ^ static_cast<unsigned int>(time(nullptr)) ^ (getpid() << 9) ^ (dia * 17) ^ (equipo_id * 41);
    std::mt19937 gen(seed);

    int objetivo_base;
    switch (equipo_id) {
        case 0: { std::uniform_int_distribution<> dis(6, 14); objetivo_base = dis(gen); break; }
        case 1: { std::uniform_int_distribution<> dis(8, 20); objetivo_base = dis(gen); break; }
        case 2: { std::uniform_int_distribution<> dis(4, 12); objetivo_base = dis(gen); break; }
        case 3: { std::uniform_int_distribution<> dis(2, 8);  objetivo_base = dis(gen); break; }
        default: objetivo_base = 5;
    }

    int unidades_finales = (objetivo_base * porcentaje) / 100;
    std::uniform_int_distribution<> bonus_dis(-1, 2);
    unidades_finales += bonus_dis(gen);

    // Permitir 0 en casos extremos de fallo absoluto
    if (unidades_finales < 0) unidades_finales = 0;
    return unidades_finales;
}

/* ---------- Función del equipo (hijo) que escribe en shared memory ---------- */
void equipo_recoleccion(int equipo_id, int shm_id, int dia) {
    pid_t pid = getpid();

    // Conectar a shared memory (con shmid pasado por el padre)
    SharedMemoryData* shared_data = (SharedMemoryData*) shmat(shm_id, NULL, 0);
    if (shared_data == (void*) -1) {
        perror("shmat (hijo)");
        exit(1);
    }

    // Marcar estado trabajando (0)
    shared_data->reportes[equipo_id].id_equipo = equipo_id;
    shared_data->reportes[equipo_id].estado_equipo = 0;
    shared_data->reportes[equipo_id].pid_equipo = pid;

    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid << "] "
         << actividades_exploracion[equipo_id] << endl;

    // Simulación de trabajo (tiempos aleatorios)
    std::random_device rd;
    unsigned int seed_sleep = rd() ^ static_cast<unsigned int>(time(nullptr)) ^ (pid << 5) ^ (dia * 13);
    std::mt19937 gen_sleep(seed_sleep);
    std::uniform_int_distribution<> sleep_dis(1, 4);

    sleep(sleep_dis(gen_sleep));

    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid << "] "
         << actividades_recoleccion[equipo_id] << endl;

    sleep(sleep_dis(gen_sleep));

    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid << "] "
         << actividades_finalizacion[equipo_id] << endl;

    sleep(1);

    // Generar resultado y unidades
    int porcentaje_exito = generar_resultado(dia, equipo_id);
    int unidades = calcular_unidades(porcentaje_exito, equipo_id, dia);

    // Escribir resultado en memoria compartida
    shared_data->reportes[equipo_id].recursos_recolectados = unidades;
    shared_data->reportes[equipo_id].estado_equipo = 1; // completado
    shared_data->reportes[equipo_id].pid_equipo = pid;

    // Incrementar contador atómicamente
    __sync_fetch_and_add(&shared_data->equipos_completados, 1);

    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid
         << "] Completo ciclo: " << unidades << " unidades obtenidas" << endl;

    // Desconectar y terminar
    if (shmdt(shared_data) == -1) {
        perror("shmdt (hijo)");
    }
    _exit(0);
}

/* ---------- Evaluación de supervivencia ---------- */
bool evaluar_supervivencia(const Recursos& totales, int& moral_perdida, string& razon_penalizacion) {
    bool sobrevive = true;
    moral_perdida = 0;
    razon_penalizacion.clear();
    vector<string> razones;

    if (totales.agua < MIN_AGUA) {
        int perdida = (MIN_AGUA - totales.agua) * 3;
        moral_perdida += perdida;
        razones.push_back("falta de agua");
        sobrevive = false;
    }
    if (totales.alimentos < MIN_ALIMENTOS) {
        int perdida = (MIN_ALIMENTOS - totales.alimentos) * 2;
        moral_perdida += perdida;
        razones.push_back("falta de comida");
        sobrevive = false;
    }
    if (totales.construccion < MIN_CONSTRUCCION) {
        int perdida = (MIN_CONSTRUCCION - totales.construccion) * 4;
        moral_perdida += perdida;
        razones.push_back("falta de refugio");
        sobrevive = false;
    }
    if (totales.senales < MIN_SENALES) {
        int perdida = (MIN_SENALES - totales.senales) * 5;
        moral_perdida += perdida;
        razones.push_back("señales insuficientes");
        sobrevive = false;
    }

    if (!razones.empty()) {
        razon_penalizacion = "por ";
        for (size_t i = 0; i < razones.size(); ++i) {
            if (i == 0) razon_penalizacion += razones[i];
            else if (i == razones.size() - 1) razon_penalizacion += " y " + razones[i];
            else razon_penalizacion += ", " + razones[i];
        }
    }
    return sobrevive;
}

/* ---------- Función coordinador (padre) ---------- */
void coordinador(int dias_simulacion) {
    int moral = MORAL_INICIAL;
    int senales_consecutivas = 0;
    bool rescate_exitoso = false;

    cout << "=== SISTEMA DE SUPERVIVENCIA ACTIVADO ===" << endl;
    cout << "Moral inicial: " << MORAL_INICIAL << "/100" << endl;
    cout << "Días de simulación: " << dias_simulacion << endl << endl;

    // Crear segmento de memoria compartida único (IPC_PRIVATE -> evita colisiones con ftok)
    int shm_id = shmget(IPC_PRIVATE, sizeof(SharedMemoryData), IPC_CREAT | 0600);
    if (shm_id == -1) {
        perror("shmget (padre)");
        exit(1);
    }

    // Mapearlo en el proceso coordinador
    SharedMemoryData* shared_data = (SharedMemoryData*) shmat(shm_id, NULL, 0);
    if (shared_data == (void*) -1) {
        perror("shmat (padre)");
        // intentar eliminar segmento
        shmctl(shm_id, IPC_RMID, NULL);
        exit(1);
    }

    // Inicializar a cero (útil si existiera basura)
    memset(shared_data, 0, sizeof(SharedMemoryData));

    for (int dia = 1; dia <= dias_simulacion && moral > 0 && !rescate_exitoso; ++dia) {
        cout << "=== DÍA " << dia << " DE SUPERVIVENCIA ===" << endl;
        cout << "Iniciando equipos de recolección..." << endl << endl;

        // Inicializar datos del día en shared memory
        shared_data->equipos_completados = 0;
        shared_data->dia_actual = dia;
        for (int i = 0; i < 4; ++i) {
            shared_data->reportes[i].id_equipo = i;
            shared_data->reportes[i].recursos_recolectados = 0;
            shared_data->reportes[i].estado_equipo = 0; // trabajando
            shared_data->reportes[i].pid_equipo = 0;
        }

        // Crear procesos hijos (equipos)
        vector<pid_t> pids(4, -1);
        for (int i = 0; i < 4; ++i) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork (padre)");
                // intentar esperar a hijos creados antes de salir
                for (int j = 0; j < i; ++j) if (pids[j] > 0) waitpid(pids[j], NULL, 0);
                // limpiar shared memory
                shmdt(shared_data);
                shmctl(shm_id, IPC_RMID, NULL);
                exit(1);
            } else if (pid == 0) {
                // Hijo: ejecutar función equipo (le paso shmid, no pointer)
                equipo_recoleccion(i, shm_id, dia);
                // nunca llega aquí
            } else {
                pids[i] = pid;
            }
        }

        // Esperar a que los 4 hijos escriban (polling rápido)
        while (shared_data->equipos_completados < 4) {
            usleep(100000); // 100 ms polling -> más responsivo y menos latencia que sleep(1)
        }

        // Leer resultados
        Recursos recursos_dia = {0,0,0,0};
        recursos_dia.agua = shared_data->reportes[0].recursos_recolectados;
        recursos_dia.alimentos = shared_data->reportes[1].recursos_recolectados;
        recursos_dia.construccion = shared_data->reportes[2].recursos_recolectados;
        recursos_dia.senales = shared_data->reportes[3].recursos_recolectados;

        // Esperar terminación de hijos (limpieza)
        for (int i = 0; i < 4; ++i) {
            if (pids[i] > 0) waitpid(pids[i], NULL, 0);
        }

        // Mostrar reportes
        cout << "\nREPORTES FINALES:" << endl;
        cout << "- Equipo Agua (PID: " << shared_data->reportes[0].pid_equipo << "): "
             << recursos_dia.agua << " unidades obtenidas" << endl;
        cout << "- Equipo Alimentos (PID: " << shared_data->reportes[1].pid_equipo << "): "
             << recursos_dia.alimentos << " unidades obtenidas" << endl;
        cout << "- Equipo Construcción (PID: " << shared_data->reportes[2].pid_equipo << "): "
             << recursos_dia.construccion << " unidades obtenidas" << endl;
        cout << "- Equipo Señales (PID: " << shared_data->reportes[3].pid_equipo << "): "
             << recursos_dia.senales << " unidades obtenidas" << endl;

        // Resultados y evaluación
        cout << "\nRESULTADOS DEL DÍA:" << endl;
        cout << "- Agua recolectada: " << recursos_dia.agua << "/" << MIN_AGUA << " ("
             << (recursos_dia.agua >= MIN_AGUA ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;
        cout << "- Alimentos obtenidos: " << recursos_dia.alimentos << "/" << MIN_ALIMENTOS << " ("
             << (recursos_dia.alimentos >= MIN_ALIMENTOS ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;
        cout << "- Materiales de construcción: " << recursos_dia.construccion << "/" << MIN_CONSTRUCCION << " ("
             << (recursos_dia.construccion >= MIN_CONSTRUCCION ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;
        cout << "- Señales mantenidas: " << recursos_dia.senales << "/" << MIN_SENALES << " ("
             << (recursos_dia.senales >= MIN_SENALES ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;

        int moral_perdida;
        string razon_penalizacion;
        bool dia_sobrevivido = evaluar_supervivencia(recursos_dia, moral_perdida, razon_penalizacion);

        moral -= moral_perdida;
        if (moral < 0) moral = 0;

        // Señales consecutivas
        if (recursos_dia.senales >= MIN_SENALES) {
            ++senales_consecutivas;
            cout << "✅ Señales activas: Día " << senales_consecutivas << " consecutivo" << endl;
        } else {
            senales_consecutivas = 0;
            cout << "❌ Señales insuficientes, contador reiniciado" << endl;
        }

        cout << "\nEstado: " << (dia_sobrevivido ? "DÍA SUPERADO" : "SUPERVIVENCIA CRÍTICA") << endl;
        cout << "Moral del grupo: " << moral << "/100";
        if (moral_perdida > 0) cout << " (-" << moral_perdida << " " << razon_penalizacion << ")";
        else cout << " (sin cambio)";
        cout << endl;

        // Condiciones de victoria/derrota
        if (senales_consecutivas >= DIAS_RESCATE) {
            cout << "\n🎉 ¡RESCATE EXITOSO! 🎉" << endl;
            cout << "Han mantenido señales activas por " << senales_consecutivas << " días consecutivos" << endl;
            rescate_exitoso = true;
            break;
        }
        if (moral <= 0) {
            cout << "\n💀 FRACASO - MORAL AGOTADA 💀" << endl;
            cout << "El grupo ha perdido la esperanza después de " << dia << " días" << endl;
            break;
        }
        if (dia == dias_simulacion && !rescate_exitoso) {
            cout << "\n⏰ LÍMITE DE TIEMPO ALCANZADO ⏰" << endl;
            cout << "Sobrevivieron " << dias_simulacion << " días con moral " << moral << "/100" << endl;
            cout << "Señales activas consecutivas: " << senales_consecutivas << "/" << DIAS_RESCATE << endl;
        }

        cout << "\n----------------------------------------\n" << endl;
        sleep(1);
    }

    if (rescate_exitoso) {
        cout << "\n✨ FELICIDADES - MISIÓN CUMPLIDA ✨" << endl;
        cout << "Todos los sobrevivientes han sido rescatados exitosamente" << endl;
    }

    // Desconectar y eliminar segmento
    if (shmdt(shared_data) == -1) {
        perror("shmdt (padre)");
    }
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl(IPC_RMID)");
    }
}

/* ---------- main ---------- */
int main(int argc, char* argv[]) {
    int dias_simulacion;

    if (argc == 2) {
        dias_simulacion = atoi(argv[1]);
        if (dias_simulacion < MIN_DIAS || dias_simulacion > MAX_DIAS) {
            cout << "Error: Los días deben estar entre " << MIN_DIAS << " y " << MAX_DIAS << endl;
            return 1;
        }
    } else if (argc == 1) {
        cout << "=== SISTEMA DE SUPERVIVENCIA ===" << endl;
        cout << "Ingrese el número de días a simular (" << MIN_DIAS << "-" << MAX_DIAS << "): ";
        cin >> dias_simulacion;
        if (cin.fail() || dias_simulacion < MIN_DIAS || dias_simulacion > MAX_DIAS) {
            cout << "Error: Debe ingresar un número entre " << MIN_DIAS << " y " << MAX_DIAS << endl;
            return 1;
        }
        cout << endl;
    } else {
        cout << "Uso: " << argv[0] << " [días_simulacion]" << endl;
        cout << "O ejecute sin argumentos para ingreso interactivo" << endl;
        cout << "Días deben estar entre " << MIN_DIAS << " y " << MAX_DIAS << endl;
        return 1;
    }

    coordinador(dias_simulacion);
    return 0;
}