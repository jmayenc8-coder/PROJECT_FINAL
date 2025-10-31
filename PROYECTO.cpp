#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <random>
#include <limits>
#include <cctype>
using namespace std;

struct RegistroEstado {
    string accion;
    string ubicacion;
    string momento;
};

struct Encomienda {
    string id;
    string remitenteEmail;
    double peso;
    bool pagada;
    string estado;
    int mensajeroId;
    vector<RegistroEstado> log;
};

// ---------- UTILIDADES ----------
string ahora() {
    time_t t = time(nullptr);
    tm *lt = localtime(&t);
    char b[64];
    strftime(b, sizeof(b), "%d/%m/%Y %H:%M:%S", lt);
    return string(b);
}

bool soloDigitos(const string &s) {
    return !s.empty() && all_of(s.begin(), s.end(), ::isdigit);
}

bool esNumeroDecimal(const string &s) {
    if (s.empty()) return false;
    int puntos = 0;
    for (char c: s) {
        if (c == '.') puntos++;
        else if (!isdigit((unsigned char)c)) return false;
        if (puntos > 1) return false;
    }
    return true;
}

string trim(const string &s) {
    size_t i = 0, j = s.size();
    while (i < j && isspace((unsigned char)s[i])) ++i;
    while (j > i && isspace((unsigned char)s[j-1])) --j;
    return s.substr(i, j - i);
}

string toLowerTrim(string s) {
    s = trim(s);
    for (char &c : s) c = tolower((unsigned char)c);
    return s;
}

int horaActual() {
    time_t t = time(nullptr);
    tm *lt = localtime(&t);
    return lt->tm_hour;
}

bool domingo() {
    time_t t = time(nullptr);
    tm *lt = localtime(&t);
    return lt->tm_wday == 0;
}

// Generador de IDs único (verifica contra envios existentes)
string generarIdUnico(const vector<Encomienda> &envios) {
    static mt19937 rng((unsigned)time(nullptr));
    uniform_int_distribution<int> dist(1000, 9999);
    string id;
    bool ok = false;
    while (!ok) {
        id = "EG-" + to_string(dist(rng));
        ok = true;
        for (const auto &e : envios) if (e.id == id) { ok = false; break; }
    }
    return id;
}

// Elimina todos los caracteres no dígitos (útil para tarjetas)
string soloDigitosTrim(const string &s) {
    string out;
    for (char c : s) if (isdigit((unsigned char)c)) out.push_back(c);
    return out;
}

// Lee un entero de forma segura y limpia el buffer de la linea.
// Devuelve true si se leyó correctamente y coloca el valor en out.
// Si falla, limpia el buffer y devuelve false.
bool leerEnteroSeguro(int &out, const string &mensaje = "") {
    if (!mensaje.empty()) cout << mensaje;
    if (!(cin >> out)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return false;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return true;
}

// ---------- CLASE BASE ----------
class Usuario {
public:
    string mail;
    string clave;
    string tipo;
    int uid;
    Usuario(string m, string c, string t, int id=0) : mail(m), clave(c), tipo(t), uid(id) {}
    virtual void menu(vector<Encomienda>&, vector<Usuario*>&, vector<pair<string,double>>&)=0;
    virtual ~Usuario() = default;
};

// ---------- CLIENTE ----------
class Cliente : public Usuario {
public:
    Cliente(string m, string c): Usuario(m,c,"Cliente") {}

    void menu(vector<Encomienda>& envs, vector<Usuario*>&, vector<pair<string,double>>& tarifas) override {
        int op;
        do {
            cout << "\n--- MENU CLIENTE ---\n1) Crear solicitud\n2) Pagar\n3) Rastrear\n4) Mis solicitudes\n5) Salir\n> ";
            if (!leerEnteroSeguro(op)) { cout << "Opcion invalida. Ingresa un numero.\n"; continue; }
            if (op == 1) crearSolicitud(envs, tarifas);
            else if (op == 2) pagar(envs);
            else if (op == 3) rastrear(envs);
            else if (op == 4) listarPropias(envs);
            else if (op != 5) cout << "Opcion no valida.\n";
        } while (op != 5);
    }

    void crearSolicitud(vector<Encomienda>& envs, vector<pair<string,double>>& tarifas) {
        if (domingo()) { cout << "No operamos los domingos.\n"; return; }

        string ciudadRaw, pesoStr;
        cout << "Ciudad (para tarifa): "; getline(cin, ciudadRaw);
        cout << "Peso (lbs): "; getline(cin, pesoStr);

        string ciudadComp = toLowerTrim(ciudadRaw);
        if (!esNumeroDecimal(pesoStr) && !soloDigitos(pesoStr)) { cout << "Peso invalido.\n"; return; }

        double peso = 0.0;
        try {
            peso = stod(pesoStr);
        } catch (...) { cout << "Peso invalido.\n"; return; }

        if (peso <= 0.0) { cout << "Peso debe ser mayor que cero.\n"; return; }
        if (peso > 15.0) { cout << "No se aceptan paquetes mayores a 15 lbs.\n"; return; }

        double base = 50.0;
        for (auto &t : tarifas) {
            if (toLowerTrim(t.first) == ciudadComp) { base = t.second; break; }
        }
        double aplicada = (horaActual() >= 8 && horaActual() < 18) ? base : base * 2.0;

        Encomienda e;
        e.id = generarIdUnico(envs);
        e.remitenteEmail = mail;
        e.peso = peso;
        e.pagada = false;
        e.estado = "Solicitado";
        e.mensajeroId = -1;
        string ubic = ciudadRaw.empty() ? "Oficina" : ciudadRaw;
        e.log.push_back({"Solicitud registrada", ubic, ahora()});
        envs.push_back(e);

        cout << fixed << setprecision(2);
        cout << "Solicitud creada [" << e.id << "] - Tarifa estimada: Q" << aplicada << "\n";
    }

    void pagar(vector<Encomienda>& envs) {
        string id; cout << "ID de la encomienda: "; getline(cin, id);
        for (auto &e : envs) {
            if (e.id == id && e.remitenteEmail == mail) {
                if (e.pagada) { cout << "Ya esta pagada.\n"; return; }
                string tarjeta; cout << "Numero de tarjeta (16 digitos, puede incluir espacios): "; getline(cin, tarjeta);
                tarjeta = soloDigitosTrim(tarjeta);
                if (tarjeta.size() != 16 || !soloDigitos(tarjeta)) { cout << "Tarjeta invalida.\n"; return; }
                e.pagada = true;
                e.log.push_back({"Pago realizado", "Sistema", ahora()});
                cout << "Pago exitoso.\n";
                return;
            }
        }
        cout << "Encomienda no encontrada.\n";
    }

    void rastrear(vector<Encomienda>& envs) {
        string id; cout << "ID: "; getline(cin, id);
        for (auto &e : envs) {
            if (e.id == id && e.remitenteEmail == mail) {
                cout << "Estado actual: " << e.estado << "\nHistorial:\n";
                for (auto &r : e.log) cout << " - " << r.momento << " | " << r.accion << " | " << r.ubicacion << "\n";
                return;
            }
        }
        cout << "No encontrada.\n";
    }

    void listarPropias(vector<Encomienda>& envs) {
        cout << "Tus solicitudes:\n";
        for (auto &e : envs) if (e.remitenteEmail == mail)
            cout << e.id << " | Peso: " << e.peso << " | Pagado: " << (e.pagada ? "Si" : "No") << " | Estado: " << e.estado << "\n";
    }
};

// ---------- MENSAJERO ----------
class Mensajero : public Usuario {
public:
    bool disponible;
    Mensajero(string m, string c, int id_): Usuario(m,c,"Mensajero", id_), disponible(true) {}

    void menu(vector<Encomienda>& envs, vector<Usuario*>& usrs, vector<pair<string,double>>& tarifas) override {
        int op;
        do {
            cout << "\n--- MENU MENSAJERO ---\n1) Ver asignaciones\n2) Marcar En Ruta\n3) Marcar Recogido\n4) Marcar Entregado\n5) Salir\n> ";
            if (!leerEnteroSeguro(op)) { cout << "Opcion invalida. Ingresa un numero.\n"; continue; }
            if (op == 1) ver(envs);
            else if (op == 2) marcarEstadoConAyuda(envs, "En ruta");
            else if (op == 3) marcarEstadoConAyuda(envs, "Recogido");
            else if (op == 4) marcarEstadoConAyuda(envs, "Entregado");
            else if (op != 5) cout << "Opcion no valida.\n";
        } while (op != 5);
    }

    void ver(vector<Encomienda>& envs) {
        cout << "Asignadas:\n";
        bool alguna = false;
        for (auto &e : envs) if (e.mensajeroId == uid) {
            cout << e.id << " | Estado: " << e.estado << " | Pagado: " << (e.pagada ? "Si" : "No") << "\n";
            alguna = true;
        }
        if (!alguna) cout << "No tienes encomiendas asignadas.\n";
    }

    // Nueva función: muestra al mensajero sus encomiendas asignadas antes de pedir ID
    void mostrarMisEncomiendas(vector<Encomienda>& envs) {
        cout << "Tus encomiendas asignadas (ID - Estado):\n";
        int contador = 0;
        for (auto &e : envs) {
            if (e.mensajeroId == uid) {
                cout << " - " << e.id << " | " << e.estado << "\n";
                ++contador;
            }
        }
        if (contador == 0) cout << "  (No tienes encomiendas asignadas en este momento)\n";
    }

    // Generaliza el flujo de marcar estado mostrando primero los IDs asignados
    void marcarEstadoConAyuda(vector<Encomienda>& envs, const string &nuevo) {
        // Mostrar las encomiendas asignadas al mensajero para que no necesite memorizar el ID
        mostrarMisEncomiendas(envs);

        // Pedir ID de la encomienda
        string id; cout << "ID: "; getline(cin, id);
        for (auto &e : envs) {
            if (e.id == id && e.mensajeroId == uid) {
                if (nuevo == "En ruta" && e.estado == "Asignado") {
                    e.estado = "En ruta"; e.log.push_back({"En ruta", "Mensajero", ahora()});
                    cout << "Estado actualizado a En ruta.\n"; return;
                }
                if (nuevo == "Recogido" && (e.estado == "En ruta" || e.estado == "Asignado")) {
                    e.estado = "Recogido"; e.log.push_back({"Recogido", "Mensajero", ahora()});
                    cout << "Estado actualizado a Recogido.\n"; return;
                }
                if (nuevo == "Entregado" && (e.estado == "Recogido" || e.estado == "En ruta")) {
                    e.estado = "Entregado"; e.log.push_back({"Entregado", "Mensajero", ahora()});
                    cout << "Estado actualizado a Entregado.\n"; return;
                }
                cout << "Transicion invalida desde " << e.estado << " a " << nuevo << "\n"; return;
            }
        }
        cout << "No asignado, ID invalido o no corresponde a tus encomiendas.\n";
    }
};

// ---------- CONTROLADOR ----------
class Controlador : public Usuario {
public:
    Controlador(string m, string c): Usuario(m,c,"Controlador") {}

    void menu(vector<Encomienda>& envs, vector<Usuario*>& usrs, vector<pair<string,double>>& tarifas) override {
        int op;
        do {
            cout << "\n--- MENU CONTROLADOR ---\n1) Ver sin asignar\n2) Asignar a mensajero\n3) Ver todo\n4) Salir\n> ";
            if (!leerEnteroSeguro(op)) { cout << "Opcion invalida. Ingresa un numero.\n"; continue; }
            if (op == 1) verSinAsignar(envs);
            else if (op == 2) asignar(envs, usrs);
            else if (op == 3) verTodos(envs);
            else if (op != 4) cout << "Opcion no valida.\n";
        } while (op != 4);
    }

    void verSinAsignar(vector<Encomienda>& envs) {
        cout << "Solicitudes pendientes:\n";
        for (auto &e : envs) if (e.estado == "Solicitado")
            cout << e.id << " | Peso: " << e.peso << " | Pagado: " << (e.pagada ? "Si" : "No") << "\n";
    }

    void asignar(vector<Encomienda>& envs, vector<Usuario*>& usrs) {
        // Mostrar lista de mensajeros disponibles primero, para que el usuario vea y elija
        cout << "Mensajeros disponibles:\n";
        int contadorMens = 0;
        for (auto u : usrs) {
            if (u->tipo == "Mensajero") {
                cout << " - ID " << u->uid << " | " << u->mail << "\n";
                ++contadorMens;
            }
        }
        if (contadorMens == 0) {
            cout << "No hay mensajeros registrados.\n";
            return;
        }

        // Mostrar lista de encomiendas no asignadas para que el usuario pueda ver los IDs disponibles
        cout << "Encomiendas sin asignar:\n";
        int contadorEnv = 0;
        for (auto &e : envs) {
            if (e.estado == "Solicitado") {
                cout << " - " << e.id << " | Peso: " << e.peso << " | Pagado: " << (e.pagada ? "Si" : "No") << "\n";
                ++contadorEnv;
            }
        }
        if (contadorEnv == 0) {
            cout << "No hay encomiendas sin asignar.\n";
            return;
        }

        // Pedir ID del paquete a asignar (el usuario ya vio la lista arriba)
        string id; cout << "ID a asignar: "; getline(cin, id);

        // Pedir ID de mensajero, el usuario ya vio la lista arriba
        int mid;
        cout << "ID Mensajero: ";
        if (!leerEnteroSeguro(mid)) { cout << "ID invalido\n"; return; }

        bool existeMens = false;
        for (auto u: usrs) if (u->tipo == "Mensajero" && u->uid == mid) { existeMens = true; break; }
        if (!existeMens) { cout << "Mensajero no existe.\n"; return; }

        for (auto &e : envs) {
            if (e.id == id && e.estado == "Solicitado") {
                if (!e.pagada) cout << "Advertencia: paquete no pagado aun, asignando igualmente.\n";
                e.mensajeroId = mid; e.estado = "Asignado";
                e.log.push_back({"Asignado a mensajero", "Controlador", ahora()});
                cout << "Asignado correctamente a mensajero " << mid << "\n";
                return;
            }
        }
        cout << "No se pudo asignar (ID no existe o ya asignado).\n";
    }

    void verTodos(vector<Encomienda>& envs) {
        cout << "Listado general:\n";
        for (auto &e : envs)
            cout << e.id << " | Estado: " << e.estado << " | Cliente: " << e.remitenteEmail << " | Mensajero: " << (e.mensajeroId == -1 ? "N/A" : to_string(e.mensajeroId)) << "\n";
    }
};

// ---------- ADMINISTRADOR ----------
class Administrador : public Usuario {
public:
    Administrador(string m, string c): Usuario(m,c,"Admin") {}

    void menu(vector<Encomienda>& envs, vector<Usuario*>& usrs, vector<pair<string,double>>& tarifas) override {
        int op;
        do {
            cout << "\n--- MENU ADMIN ---\n1) Crear usuario\n2) Listar mensajeros\n3) Agregar/Modificar tarifa por ciudad\n4) Reporte simple\n5) Salir\n> ";
            if (!leerEnteroSeguro(op)) { cout << "Opcion invalida. Ingresa un numero.\n"; continue; }
            if (op == 1) crearUsuario(usrs);
            else if (op == 2) listarMensajeros(usrs);
            else if (op == 3) editarTarifa(tarifas);
            else if (op == 4) reporte(envs);
            else if (op != 5) cout << "Opcion no valida.\n";
        } while (op != 5);
    }

    void crearUsuario(vector<Usuario*>& usrs) {
        string rolRaw, mail, pass;
        cout << "Rol (Cliente/Controlador/Mensajero/Admin): "; getline(cin, rolRaw);
        cout << "Email: "; getline(cin, mail);
        cout << "Password: "; getline(cin, pass);

        string rol = toLowerTrim(rolRaw);
        mail = trim(mail); pass = trim(pass);
        if (mail.empty() || pass.empty()) { cout << "Email o password vacio.\n"; return; }

        if (rol == "cliente") {
            usrs.push_back(new Cliente(mail, pass));
            cout << "Usuario Cliente creado: " << mail << "\n";
        }
        else if (rol == "controlador") {
            usrs.push_back(new Controlador(mail, pass));
            cout << "Usuario Controlador creado: " << mail << "\n";
        }
        else if (rol == "mensajero") {
            int nid = 1;
            for (auto u: usrs) if (u->tipo == "Mensajero") nid = max(nid, dynamic_cast<Mensajero*>(u)->uid + 1);
            usrs.push_back(new Mensajero(mail, pass, nid));
            cout << "Mensajero creado con ID " << nid << " y email " << mail << "\n";
        }
        else if (rol == "admin") {
            usrs.push_back(new Administrador(mail, pass));
            cout << "Usuario Admin creado: " << mail << "\n";
        }
        else cout << "Rol incorrecto.\n";
    }

    void listarMensajeros(vector<Usuario*>& usrs) {
        cout << "Mensajeros registrados:\n";
        for (auto u: usrs) if (u->tipo == "Mensajero")
            cout << " - ID " << u->uid << " | " << u->mail << "\n";
    }

    void editarTarifa(vector<pair<string,double>>& tarifas) {
        cout << "Tarifas actuales:\n";
        for (size_t i = 0; i < tarifas.size(); ++i) {
            cout << " " << (i+1) << ") Ciudad: " << tarifas[i].first
                 << " | Tarifa base: Q" << fixed << setprecision(2) << tarifas[i].second << "\n";
        }
        string ciudadRaw; cout << "Ciudad a modificar/crear: "; getline(cin, ciudadRaw);
        string ciudadComp = toLowerTrim(ciudadRaw);
        string val; cout << "Nueva tarifa base (Q): "; getline(cin, val);
        if (!esNumeroDecimal(val) && !soloDigitos(val)) { cout << "Valor invalido.\n"; return; }

        double v = 0.0;
        try { v = stod(val); } catch (...) { cout << "Valor invalido.\n"; return; }
        if (v < 0.0) { cout << "La tarifa debe ser no negativa.\n"; return; }

        bool mod = false;
        for (auto &t : tarifas) {
            if (toLowerTrim(t.first) == ciudadComp) { t.second = v; mod = true; break; }
        }
        if (mod) {
            cout << "Tarifa modificada para la ciudad '" << (ciudadRaw.empty() ? "Default" : ciudadRaw) << "'. Nuevo valor: Q" << fixed << setprecision(2) << v << "\n";
        } else {
            tarifas.push_back({ciudadRaw.empty() ? "Default" : ciudadRaw, v});
            cout << "Tarifa agregada para la ciudad '" << (ciudadRaw.empty() ? "Default" : ciudadRaw) << "'. Valor: Q" << fixed << setprecision(2) << v << "\n";
        }
    }

    void reporte(vector<Encomienda>& envs) {
        int s=0,a=0,er=0,r=0,e=0;
        for (auto &en : envs) {
            if (en.estado == "Solicitado") s++;
            else if (en.estado == "Asignado") a++;
            else if (en.estado == "En ruta") er++;
            else if (en.estado == "Recogido") r++;
            else if (en.estado == "Entregado") e++;
        }
        cout << "Reporte: Solicitados: " << s << "\nAsignados: " << a << "\nEn_ruta: " << er << "\nRecogidos: " << r << "\nEntregados: " << e << "\n";
    }
};

// ---------- PROGRAMA PRINCIPAL ----------
int main() {
    vector<Usuario*> usuarios;
    vector<Encomienda> envios;
    vector<pair<string,double>> tarifas;
    tarifas.push_back({"Guatemala", 50.0});

    // usuarios base (solicitado por el usuario)
    usuarios.push_back(new Cliente("Juan", "12345"));
    usuarios.push_back(new Administrador("Javier", "12345"));
    usuarios.push_back(new Controlador("Mario", "12345"));
    usuarios.push_back(new Mensajero("Carlos", "12345", 1));

    cout << "Envios Garantizados - prototipo\n";

    while (true) {
        int op;
        cout << "\n--- MENU PRINCIPAL ---\n1) Iniciar sesion\n2) Salir\n> ";
        if (!leerEnteroSeguro(op)) { cout << "Entrada invalida. Por favor ingresa un numero.\n"; continue; }

        if (op == 1) {
            string mail, pass;
            cout << "Email: "; getline(cin, mail);          
            cout << "Password: "; getline(cin, pass);
            mail = trim(mail); pass = trim(pass);
            if (mail.empty() || pass.empty()) { cout << "Email o password vacio.\n"; continue; }

            Usuario* activo = nullptr;
            for (auto u: usuarios) if (u->mail == mail && u->clave == pass) { activo = u; break; }
            if (!activo) { cout << "Credenciales incorrectas. Intenta de nuevo.\n"; continue; }

            cout << "\nBienvenido " << activo->mail << " (" << activo->tipo << ")\n";
            activo->menu(envios, usuarios, tarifas);
        }
        else if (op == 2) {
            break;
        }
        else {
            cout << "Opcion no valida. Por favor elige 1 o 2.\n";
        }
    }

    for (auto u: usuarios) delete u;
    cout << "Adios.\n";
    return 0;
}